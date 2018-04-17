#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

#define NUM_SYMBOLS 256
enum vid_quality {HD, SD};
enum vid_quality MODE = SD;

/**
 * Get the number of bytes in a frame
 */
int get_frame_size() {
    return MODE == HD ? 1280 * 720 : 800 * 450;
}

/**
 * Get the address of the first byte of a specified frame (zero-indexed)
 */
uint8_t * get_frame_start(int frame_number, uint8_t * basis) {
    return basis + frame_number * get_frame_size();
}

int get_num_frames(struct stat * source_info) {
    return source_info->st_size / get_frame_size();
}

struct huff_node {
    struct huff_node * left;
    struct huff_node * right;
    long occur;
    uint8_t symbol;
};

/**
 * Quicksort (qsort) comparator
 * Operates in reverse as quicksort does acending by default
 */
int sortby_occurances(const void * elem1, const void * elem2) {
    long first_occur = (*(struct huff_node**)elem1)->occur;
    long second_occur = (*(struct huff_node**)elem2)->occur;
    if (first_occur < second_occur)
        return 1;
    if (first_occur > second_occur)
        return -1;
    return 0; // Equal
}

void recurse_print(int indent, struct huff_node * head) {
        // Print order: occurances, sym if applicable
        if (head == NULL)
                return;
        for (int i = 0; i < indent; i++)
                printf("|  ");
        if (head->left == NULL && head->right == NULL)
            printf("%ld (%d)\n", head->occur, head->symbol);
        if (head->right != NULL || head->left != NULL)
            printf("%ld\n", head->occur);
        recurse_print(indent + 1, head->left);
        recurse_print(indent + 1, head->right);
        return;
}

void debug_print(struct huff_node * head) {
        printf("\n");
        recurse_print(0, head);
        printf("\n");
}

int PRINT = 0;

int main(int argv, char ** argc) {
    // Parse arguments, open file, and get size
    if (argv < 3) {
        printf("Usage: %s <input_file_name> <output_file_name> [--hd] [--print]\n", argc[0]);
        return 1;
    }
    if (argv >= 4) {
        if (strcmp(argc[3], "--hd") == 0)
            MODE = HD;
        if (strcmp(argc[3], "--print") == 0)
            PRINT = 1;
    }
    if (argv >= 5) {
        if (strcmp(argc[4], "--hd") == 0)
            MODE = HD;
        if (strcmp(argc[4], "--print") == 0)
            PRINT = 1;
    }
    int source_fd = open(argc[1], O_RDONLY);
    if (source_fd == -1) {
        printf("Unable to open file '%s' for reading: %s\n", argc[1], strerror(errno));
        return 1;
    }
    struct stat source_info;
    if (fstat(source_fd, &source_info) == -1) {
        printf("fstat error during setup: %s\n", strerror(errno));
        return 1;
    }
    uint8_t * source_data = mmap(NULL, source_info.st_size, PROT_READ, MAP_SHARED, source_fd, 0);
    if (source_data == MAP_FAILED) {
        printf("Unable to mmap source file: %s\n", strerror(errno));
        return 1;
    }
    // Compute all frames >1 as diffs off frame 1 and count
    printf("Loaded '%s'. Generating frame diffs and probabilities...\n", argc[1]);
    uint8_t (*frame_diffs)[get_frame_size()] = malloc(get_num_frames(&source_info) * get_frame_size());
    unsigned long occurances[NUM_SYMBOLS] = {0};
    for (int frame = 1; frame < get_num_frames(&source_info); frame++) {
        uint8_t * prev_frame = get_frame_start(frame - 1, source_data);
        uint8_t * curr_frame = get_frame_start(frame, source_data);
        for (int pixel = 0; pixel < get_frame_size(); pixel++) {
            uint8_t diff = prev_frame[pixel] - curr_frame[pixel];
            frame_diffs[frame - 1][pixel] = diff;
            occurances[diff]++;
        }
    }
    // Compute easily-est compressible choice of back-differentials
    printf("Computing best differential playback series (GOT)...\n");
    uint8_t (*better_frame_diffs)[get_frame_size()] = malloc((get_num_frames(&source_info) - 1) * get_frame_size());
    unsigned int better_frame_diff_basis[get_num_frames(&source_info) - 1];
    for (int curr_diff = 0; curr_diff < get_num_frames(&source_info) - 1; curr_diff++) {
        // Compute deviation of a direct differential from the global differential
        int best_prior_diff = curr_diff;
        unsigned long base_occurances[NUM_SYMBOLS] = {0};
        for (int pixel = 0; pixel < get_frame_size(); pixel++)
            base_occurances[(uint8_t)frame_diffs[curr_diff][pixel]]++;
        unsigned long mismatch_count = 0;
        for (int i = 0; i < NUM_SYMBOLS; i++)
            mismatch_count += abs(occurances[i] - base_occurances[i] * get_num_frames(&source_info));
        // Save/make space for some statistical-tracking variables
        unsigned long orig_mismatch_count = mismatch_count;
        unsigned long trailing_mismatch_count = -1; // Underflow to max of range
        // Minimize deviation by exhaustive search for fitting basis
        for (int backtest_fdiff_idx = 0; backtest_fdiff_idx < curr_diff; backtest_fdiff_idx++) {
            unsigned long local_occurances[NUM_SYMBOLS] = {0};
            for (int pixel = 0; pixel < get_frame_size(); pixel++)
                local_occurances[(uint8_t)(frame_diffs[curr_diff][pixel] - frame_diffs[backtest_fdiff_idx][pixel])]++;
            unsigned long local_mismatch_count = 0;
            for (int i = 0; i < NUM_SYMBOLS; i++)
                local_mismatch_count += abs(occurances[i] - local_occurances[i] * get_num_frames(&source_info));
            if (local_mismatch_count < mismatch_count) {
                best_prior_diff = backtest_fdiff_idx;
                mismatch_count = local_mismatch_count;
            }
            // Update stats
            if (local_mismatch_count < trailing_mismatch_count)
                trailing_mismatch_count = local_mismatch_count;
        }
        // Record the residual and basis index
        for (int p = 0; p < get_frame_size(); p++)
            better_frame_diffs[curr_diff][p] = frame_diffs[curr_diff][p] - frame_diffs[best_prior_diff][p];
        better_frame_diff_basis[curr_diff] = best_prior_diff;
        if (PRINT) {
            if (mismatch_count == orig_mismatch_count)
                printf("No more efficient diff available for diff %d (best competing is %lu vs %lu)\n", curr_diff, trailing_mismatch_count, mismatch_count);
            else
                printf("Most efficient prior diff for diff %d is diff %d at cost %lu vs original cost %lu\n", curr_diff, best_prior_diff, mismatch_count, orig_mismatch_count);
        }
    }
    for (int i = 0; PRINT && i < NUM_SYMBOLS; i++)
        printf("Original occurances of %d: %lu\n", i, occurances[i]);
    // Load new occurance mapping
    memset(occurances, 0, sizeof(occurances));
    for (int frame = 0; frame < get_num_frames(&source_info) - 1; frame++) {
        for (int pixel = 0; pixel < get_frame_size(); pixel++) {
            occurances[better_frame_diffs[frame][pixel]]++;
        }
    }
    for (int i = 0; PRINT && i < NUM_SYMBOLS; i++)
        printf("Improved occurances of %d: %lu\n", i, occurances[i]);
    printf("Generating Huffman tree...\n");
    // Setup memory for the Huffman tree
    struct huff_node unordered_nodes[512] = {0};
    struct huff_node * ordered_nodes[NUM_SYMBOLS];
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        unordered_nodes[i].symbol = i;
        unordered_nodes[i].occur = occurances[i];
        ordered_nodes[i] = &unordered_nodes[i];
    }
    // Generate the Huffman tree
    int back_idx_unordered = NUM_SYMBOLS;
    int back_idx_ordered = NUM_SYMBOLS - 1;
    while (back_idx_ordered > 0) {
        qsort(ordered_nodes, back_idx_ordered + 1, sizeof(struct huff_node *), sortby_occurances);
        // Overwrite the left node with the new inner node after saving the left node elsewhere
        struct huff_node * inner_node = ordered_nodes[back_idx_ordered - 1];
        struct huff_node * left_node = &unordered_nodes[back_idx_unordered];
        struct huff_node * right_node = ordered_nodes[back_idx_ordered];
        *left_node = *inner_node;
        inner_node->left = left_node;
        inner_node->right = right_node;
        // Since the inner node used to be the left node, we can exploit that it
        // already has half the occurance count
        inner_node->occur += right_node->occur;
        // Inner nodes are detected by the presence of the left/right pointers,
        // so there is no need to overwrite the symbol field of the inner node
        // with some sort of flag value.
        back_idx_ordered--;
        back_idx_unordered++;
    }
    if (PRINT)
        debug_print(ordered_nodes[0]);
    // Generate the code table
    printf("Generating code table...\n");
    struct huff_code {
        uint8_t * code;
        unsigned int code_length;
    };
    uint8_t * emit_zero(uint8_t * buffer, unsigned int code_length) {
        if (code_length % 8 == 0) {
            uint8_t * new_buffer = realloc(buffer, code_length / 8 + 1);
            new_buffer[code_length / 8] = 0x00;
            return new_buffer;
        }
        return buffer;
    }
    uint8_t * branch_and_emit_one(uint8_t * buffer, unsigned int code_length) {
        // Tree branches at each step, so we have to copy the current buffer to one side and extend it on the other
        uint8_t * new_buffer = malloc(code_length / 8 + 1);
        if (code_length % 8 == 0) {
            if (buffer != NULL)
                memcpy(new_buffer, buffer, code_length / 8);
            new_buffer[code_length / 8] = 0x80;
            return new_buffer;
        }
        else {
            memcpy(new_buffer, buffer, code_length / 8 + 1);
            uint8_t next_byte = 0x80 >> (code_length % 8);
            new_buffer[code_length / 8] |= next_byte;
            return new_buffer;
        }
    }
    void recurse_gen_codes(unsigned int depth, uint8_t *  buffer, struct huff_node * head, struct huff_code * codes) {
        // If we're a leaf node, save our symbol and associated bit string
        // Memory sort of magically works out, because we recure the same number
        // of times as there are code table entries
        if (head->left == NULL && head->right == NULL) {
            codes[head->symbol].code_length = depth;
            codes[head->symbol].code = buffer;
        }
        else {
            uint8_t * left_buffer = branch_and_emit_one(buffer, depth);
            uint8_t * right_buffer = emit_zero(buffer, depth);
            recurse_gen_codes(depth + 1, left_buffer, head->left, codes);
            recurse_gen_codes(depth + 1, right_buffer, head->right, codes);
        }
    }
    struct huff_code codes[NUM_SYMBOLS] = {0};
    recurse_gen_codes(0, NULL, ordered_nodes[0], codes);
    if (PRINT)
        for (int i = 0; i < NUM_SYMBOLS; i++) {
            printf("Symbol %d encoded as ", i);
            int end_bit_idx = codes[i].code_length - 1;
            // Print leading bytes
            for (int j = 0; j < end_bit_idx / 8; j++)
                printf(BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(codes[i].code[j]));
            // Print last byte to actual symbol length
            uint8_t last_byte = codes[i].code[end_bit_idx / 8];
            for (int j = 0; j < end_bit_idx % 8 + 1; j++) {
                putc((last_byte & 0x80) ? '1' : '0', stdout);
                last_byte <<= 1;
            }
            printf(" (len %d)\n", codes[i].code_length);
        }
    // Compute lowest-cost differential approach
    /*printf("Computing most efficient base...\n");
    unsigned long total_cost = 0;
    for (int curr_frame = 1; curr_frame < get_num_frames(&source_info); curr_frame++) {
        // Try computing differentials against every frame
        // Compute the cost of using each prior frame and use the cheapest one
        int best_prior_frame = 0;
        unsigned long cost = -1; // Underflow to max
        for (int prior_frame = 0; prior_frame < curr_frame; prior_frame++) {
            uint8_t * prev_frame_idx = get_frame_start(prior_frame, source_data);
            uint8_t * curr_frame_idx = get_frame_start(curr_frame, source_data);
            int new_cost = 0;
            for (int pixel = 0; pixel < get_frame_size(); pixel++)
                new_cost += codes[(uint8_t)(prev_frame_idx[pixel] - curr_frame_idx[pixel])].code_length;
            if (new_cost < cost) {
                best_prior_frame = prior_frame;
                cost = new_cost;
            }
        }
        total_cost += cost;
        printf("Most efficient prior frame for frame %d is frame %d at cost %lu\n", curr_frame, best_prior_frame, cost);
    }
    total_cost /= 8; // Convert output size to bytes
    total_cost += get_frame_size(); // Add space needed for first frame
    printf("Optimized ompression ratio: %f (%ld bytes in, %ld bytes out)\nSaving file...\n", (float)total_cost / (float)source_info.st_size, source_info.st_size, total_cost);
    */
    printf("Computing second-pass GOT ...\n");
    unsigned long total_MLD_cost = 0;
    for (int curr_diff = 0; curr_diff < get_num_frames(&source_info) - 1; curr_diff++) {
        // Compute the cost of a direct differential
        int best_prior_diff = curr_diff;
        unsigned long best_cost = 0;
        for (int pixel = 0; pixel < get_frame_size(); pixel++)
            best_cost += codes[better_frame_diffs[curr_diff][pixel]].code_length;
        // Save/make space for some statistical-tracking variables
        unsigned long orig_best_cost = best_cost;
        unsigned long trailing_best_cost = -1; // Underflow to max of range
        // Look at each past frame differential, and find the best match to this differential
        for (int backtest_fdiff_idx = 0; backtest_fdiff_idx < curr_diff; backtest_fdiff_idx++) {
            unsigned long backtest_fdiff_cost = 0;
            for (int pixel = 0; pixel < get_frame_size(); pixel++)
                backtest_fdiff_cost += codes[(uint8_t)abs(better_frame_diffs[curr_diff][pixel] - better_frame_diffs[backtest_fdiff_idx][pixel])].code_length;
            if (backtest_fdiff_cost < best_cost) {
                best_prior_diff = backtest_fdiff_idx;
                best_cost = backtest_fdiff_cost;
            }
            // Update stats
            if (backtest_fdiff_cost < trailing_best_cost)
                trailing_best_cost = backtest_fdiff_cost;
        }
        total_MLD_cost += best_cost;
        if (PRINT) {
            if (orig_best_cost == best_cost)
                printf("No more efficient diff available for diff %d (best competing is %lu vs original cost %lu)\n", curr_diff, trailing_best_cost, orig_best_cost);
            else
                printf("Most efficient prior diff for diff %d is diff %d at cost %lu vs original cost %lu\n", curr_diff, best_prior_diff, best_cost, orig_best_cost);
        }
    }
    total_MLD_cost /= 8; // Convert output size to bytes
    total_MLD_cost += get_frame_size(); // Add space needed for first frame
    total_MLD_cost += 2 * (get_num_frames(&source_info) - 1) * sizeof(unsigned int); // Add space needed for both levels of basis-pointers
    printf("Optimized compression ratio: %f (%ld bytes in, %ld bytes out)\n", (float)total_MLD_cost / (float)source_info.st_size, source_info.st_size, total_MLD_cost);
    // Compute output file size
    unsigned long out_size = 0;
    for (int i = 0; i < NUM_SYMBOLS; i++)
        out_size += occurances[i] * codes[i].code_length;
    out_size /= 8; // Convert output size to bytes
    out_size += get_frame_size(); // Add space needed for first frame
    out_size += (get_num_frames(&source_info) - 1) * sizeof(unsigned int); // Add space needed for basis-pointers
    printf("Compression ratio: %f (%ld bytes in, %ld bytes out)\nSaving file...\n", (float)out_size / (float)source_info.st_size, source_info.st_size, out_size);
    // Setup output
    int out_fd = open(argc[2], O_RDWR | O_CREAT);
    if (out_fd == -1) {
        printf("Unable to open file '%s' for writing: %s\n", argc[2], strerror(errno));
        return 1;
    }
    if (ftruncate(out_fd, out_size) == -1) {
        printf("Unable to expand file '%s' to %ld bytes: %s\n", argc[2], out_size, strerror(errno));
        return 1;
    }
    uint8_t * out_data = mmap(NULL, out_size, PROT_WRITE, MAP_SHARED, out_fd, 0);
    if (out_data == MAP_FAILED) {
        printf("Unable to mmap output file: %s\n", strerror(errno));
        return 1;
    }
    // Encode all frames (excluding the first) using Huffman code table
    
    // Cleanup
    printf("Done! Cleaning up...\n");
    munmap(source_data, source_info.st_size);
    munmap(out_data, out_size);
    for (int i = 0; i < NUM_SYMBOLS; i++)
        free(codes[i].code);
    free(frame_diffs);
    free(better_frame_diffs);
    return 0;
}
