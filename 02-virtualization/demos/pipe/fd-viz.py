# To debug a multi-process program:
# (gdb) set detach-on-fork off
# (gdb) set follow-fork-mode parent

import gdb
import os
import re
import hashlib

class FDVisualizer(gdb.Command):
    def __init__(self):
        # Register as a GDB command so you can manually run 'mermaid-fds' if needed
        super(FDVisualizer, self).__init__("mermaid-fds", gdb.COMMAND_USER)
        # Hook the script to GDB's stop event
        gdb.events.stop.connect(self.on_stop)
        self.output_file = "fd_state.md"
        self.enabled = True
        print("Mermaid FD Visualizer loaded! Diagram will be written to 'fd_state.md' on every stop.")
        print("Tip: Open 'fd_state.md' in an IDE (like VSCode with a Mermaid extension) to watch it live-update.")

    def invoke(self, arg, from_tty):
        if arg.lower() == "off":
            self.enabled = False
            print("Auto-drawing disabled.")
        elif arg.lower() == "on":
            self.enabled = True
            print("Auto-drawing enabled.")
        else:
            self.generate_plot()

    def on_stop(self, event):
        if self.enabled:
            self.generate_plot()

    def generate_plot(self):
        links = []
        pipes = set()
        files = {}
        
        # Iterate over all active inferiors (processes)
        for inf in gdb.inferiors():
            if inf.pid == 0: continue
            pid = inf.pid
            
            fd_dir = f"/proc/{pid}/fd"
            if not os.path.exists(fd_dir): continue
            
            try:
                for fd_str in sorted(os.listdir(fd_dir), key=int):
                    fd = int(fd_str)
                    if fd >= 100: continue
                    target = os.readlink(f"{fd_dir}/{fd}")
                    
                    # Read FD flags to determine Read/Write mode
                    mode = "unknown"
                    try:
                        with open(f"/proc/{pid}/fdinfo/{fd}", "r") as f:
                            for line in f:
                                if line.startswith("flags:"):
                                    flags = int(line.split()[1], 8)
                                    m = flags & 0o3 # Mask O_ACCMODE
                                    if m == 0: mode = "read"
                                    elif m == 1: mode = "write"
                                    elif m == 2: mode = "rw"
                                    break
                    except Exception:
                        pass
                    
                    # Categorize target
                    pipe_match = re.match(r"(pipe|socket):\[(\d+)\]", target)
                    if pipe_match:
                        res_type = pipe_match.group(1)
                        inode = pipe_match.group(2)
                        pipes.add((res_type, inode))
                        links.append((pid, fd, res_type, inode, mode))
                    else:
                        # Hash the filepath to create a safe Mermaid Node ID
                        file_id = "f_" + hashlib.md5(target.encode()).hexdigest()[:8]
                        files[file_id] = target
                        links.append((pid, fd, "file", file_id, mode))
            except Exception as e:
                print(f"FD Viz Error reading process {pid}: {e}")

        self.write_mermaid(links, pipes, files)

    def write_mermaid(self, links, pipes, files):
        lines = [
            "```mermaid",
            "graph LR",
            "    %% Styles",
            "    classDef pipe fill:#f96,stroke:#333,stroke-width:2px;",
            "    classDef file fill:#8ab4f8,stroke:#333,stroke-width:1px,color:#000;",
            "    classDef fdNode fill:#eee,stroke:#999,stroke-width:1px;",
            ""
        ]

        # 1. Declare central shared resources (Files, Pipes, Sockets)
        for res_type, inode in pipes:
            if res_type == "pipe":
                # Draw pipe as a Cylinder/Database shape
                lines.append(f'    pipe_{inode}[("Pipe {inode}")]:::pipe')
            else:
                lines.append(f'    pipe_{inode}(("Socket {inode}")):::pipe')
                
        for fid, path in files.items():
            safe_path = path.replace('"', '\\"') # Escape quotes
            lines.append(f'    {fid}["{safe_path}"]:::file')
            
        lines.append("")

        # 2. Group FDs by Process and build "Flat Arrays"
        pid_to_fds = {}
        for link in links:
            pid = link[0]
            if pid not in pid_to_fds: pid_to_fds[pid] = []
            pid_to_fds[pid].append(link)

        for pid, fds in pid_to_fds.items():
            lines.append(f'    subgraph proc_{pid}["Process PID: {pid}"]')
            
            # Create FD nodes
            for _, fd, _, _, _ in fds:
                lines.append(f'        fd_{pid}_{fd}["FD {fd}"]:::fdNode')
            
            # Link FDs invisibly to force them into a vertical "flat array" stack
            for i in range(len(fds) - 1):
                fd_curr = fds[i][1]
                fd_next = fds[i+1][1]
                lines.append(f'        fd_{pid}_{fd_curr} ~~~ fd_{pid}_{fd_next}')
                
            lines.append('    end\n')

        # 3. Create directional edges
        for pid, fd, t_type, t_id, mode in links:
            fd_node = f"fd_{pid}_{fd}"
            target_node = f"pipe_{t_id}" if t_type in ("pipe", "socket") else t_id
            
            if t_type in ("pipe", "socket"):
                # Real data flow visualization
                if mode == "read":
                    lines.append(f'    {target_node} -->|read| {fd_node}')
                elif mode == "write":
                    lines.append(f'    {fd_node} -->|write| {target_node}')
                else:
                    lines.append(f'    {fd_node} <-->|rw| {target_node}')
            else:
                # Standard files
                lines.append(f'    {fd_node} -->|{mode}| {target_node}')

        lines.append("```\n")

        # Write to file
        try:
            with open(self.output_file, "w") as f:
                f.write("\n".join(lines))
            print(f"-> FD Mermaid diagram updated in {self.output_file}")
        except Exception as e:
            print(f"Failed to write diagram: {e}")

# Instantiate the command
FDVisualizer()
