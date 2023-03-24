#include "../include/mcts.h"
#include "../include/tictactoe.h"

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include <stdio.h>

static void print_board(char board[3][3])
{
    printf("\n   a b c\n");
    for (int y = 0; y < 3; y++) {
        printf("%d ", 1 + y);
        for (int x = 0; x < 3; x++) {
            printf("|");
            printf("%c", board[y][x]);
        }
        printf("|");
        printf("\n");
    }
    printf("\n");
}

#define UCT_CONSTANT 1.41421356237  // sqrt(2)

struct Node {
    char board[3][3];
    struct Move moves[9];
    struct Node* parent;
    struct Node** children;
    struct Move current_move;
    char current_player;
    int children_count;
    int visit_count;
    double value;
};

static double UCT(struct Node* node);
static struct Node* select(struct Node* root);
static struct Node* expand(struct Node *node);
static int simulate(struct Node *node, char ai_player);
static void backpropagate(struct Node *node, double reward);
static struct Move choose_best_move(struct Node *root);
static void free_tree(struct Node *node);

struct Move mcts_find_best_move(char board[3][3], char ai_player)
{
    srand(time(NULL));
    // initialize the root node
    struct Node* root = (struct Node*)malloc(sizeof(struct Node));
    memcpy(root->board, board, sizeof(root->board));
    root->current_player = (ai_player == 'X') ? 'O' : 'X';  // set the opposite at root
    root->children_count = get_valid_moves(root->board, root->moves);
    root->visit_count = 0;
    root->value = 0.0;
    root->current_move = (struct Move){-1, -1};
    root->parent = NULL;
    root->children = (struct Node**)malloc(root->children_count * sizeof(struct Node*));
    for (int i = 0; i < root->children_count; i++) {
        root->children[i] = NULL;
    }

    // perform MCTS in limited time (1 sec)
    //time_t start_time = time(NULL);
    // while (time(NULL) - start_time < 1) {
    int max_iterations = 1000;
    for (int i = 0; i < max_iterations; i++) {
        struct Node* node = select(root);
        struct Node* child = expand(node);
        if (child == NULL) continue;
        //printf("Simulating... x:%d,y:%d p:%c\n", child->current_move.x, child->current_move.y, child->current_player);
        double score = simulate(child, ai_player);
        //printf("Score: %f\n\n", score);
        backpropagate(node, score);
    }

    // for (int i = 0; i < root->children_count; i++) {
    //     printf("Move: %d %d, score: %f, visit count: %d\n", root->moves[i].x, root->moves[i].y, root->children[i]->value, root->children[i]->visit_count);
    // }
    // print_board(root->board);
    // printf("MCTS: %d simulations\n", root->visit_count);

    // choose the best move based on the highest reward
    struct Move best_move = choose_best_move(root);
    free_tree(root);  // recursively free the tree

    return best_move;
}

static struct Node* select(struct Node* root)
{
    struct Node* node = root;
    while (check_winner(node->board) == PLAYING) {
        double best_uct = -INFINITY;
        struct Node* best_child = node->children[0];

        for (int i = 0; i < node->children_count; i++) {
            if (node->children[i] == NULL) {
                return node;
            }
            struct Node* child = node->children[i];
            double uct = UCT(child);
            //printf("UCT: %f move: x:%d, y:%d p:%c\n", uct, child->current_move.x, child->current_move.y, child->current_player);
            if (uct > best_uct) {
                best_uct = uct;
                best_child = child;
            }
        }
        node = best_child;
    }
    return node;
}

static struct Node *expand(struct Node *node)
{
    if (node->children_count == 0) return NULL;  // game over (no more moves)

    int i = 0;
    while (node->children[i] != NULL) {
        i++;
    }
    node->children[i] = (struct Node*)malloc(sizeof(struct Node));

    struct Node* child = node->children[i];
    memcpy(child->board, node->board, sizeof(node->board));
    child->current_player = (node->current_player == 'X') ? 'O' : 'X';  // set the opposite of previous
    child->current_move = node->moves[rand() % node->children_count];
    child->board[child->current_move.y][child->current_move.x] = child->current_player;  // simulate move
    child->children_count = get_valid_moves(child->board, child->moves);
    child->visit_count = 0;
    child->value = 0.0;
    child->parent = node;
    child->children = (struct Node**)malloc(child->children_count * sizeof(struct Node*));
    for (int i = 0; i < child->children_count; i++) {
        child->children[i] = NULL;
    }
    //node->children[i] = child;

    return child;
}

static int simulate(struct Node *node, char ai_player)
{
    if (node == NULL) return 0;

    char board[3][3];  // copy the board state
    memcpy(board, node->board, sizeof(node->board));
    // printf("%c%d %c\n", node->current_move.x + 'a', node->current_move.y, node->current_player);
    // print_board(board);

    // simulate a game from the current board state
    char current_player = (node->current_player == 'X') ? 'O' : 'X';
    while (true) {
        enum Winner winner = check_winner(board);
        if (winner == X_WIN) return (ai_player == 'X') ? 1 : -1;
        else if (winner == O_WIN) return (ai_player == 'O') ? 1 : -1;
        else if (winner == DRAW) return 0;

        // choose a random move
        struct Move moves[9];
        int move_count = get_valid_moves(board, moves);
        struct Move random_move = moves[rand() % move_count];

        board[random_move.y][random_move.x] = current_player;  // make random move
        current_player = (current_player == 'X') ? 'O' : 'X';  // switch to the other player
    }
}

static void backpropagate(struct Node *node, double reward)
{
    // update the total reward and visit count for the current node
    while (node != NULL) {
        node->visit_count++;
        node->value += reward;
        node = node->parent;
    }
}

static struct Move choose_best_move(struct Node *root)
{
    struct Move best_move = {-1, -1};
    double best_score = -INFINITY;
    for (int i = 0; i < root->children_count; i++) {

        struct Node* child = root->children[i];
        if (child != NULL) {
            double score = child->value / child->visit_count;
            if (score > best_score) {
                best_move = child->current_move;
                best_score = score;
            }
        } else {
            break;
        }
    }
    return best_move;
}

static double UCT(struct Node* node)
{
    //if (node->visit_count == 0) return INFINITY; // node is not expanded yet

    double exploitation = node->value / node->visit_count;
    double exploration = UCT_CONSTANT * sqrt(log(node->parent->visit_count) / node->visit_count);
    return exploitation + exploration;
}

static void free_tree(struct Node *node)
{
    if (node == NULL) return;

    if (node->children != NULL) {
        for (int i = 0; i < node->children_count; i++) {
            free_tree(node->children[i]);
        }
        free(node->children);
    }
    free(node);
}
