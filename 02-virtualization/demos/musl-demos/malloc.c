#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N (100 * 1024 * 1024)

// A simple linked list node
typedef struct Node {
    int value;
    struct Node* next;
} Node;

// Insert a new node at the head of the list
Node* insert(Node* head, int value) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (!node) {
        printf("malloc failed\n");
        exit(1);
    }
    node->value = value;
    node->next = head;
    return node;
}

// Print all list elements
void print_list(Node* head) {
    Node* curr = head;
    while (curr) {
        printf("%d ", curr->value);
        curr = curr->next;
    }
    printf("\n");
}

// Free the list memory
void free_list(Node* head) {
    Node* curr = head;
    while (curr) {
        Node* tmp = curr;
        curr = curr->next;
        free(tmp);
    }
}

int main() {
    Node* head = NULL;
    for (int i = 0; i < N; ++i) {
        head = insert(head, i * 10);
    }

    printf("List contents: ");
    print_list(head);

    // Free all nodes
    free_list(head);

    return 0;
}
