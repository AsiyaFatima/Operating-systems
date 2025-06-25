#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef enum { FREE, ALLOCATED } Status;

typedef struct MemoryBlock {
    int start;
    int size;
    Status status;
    struct MemoryBlock *next;
} MemoryBlock;

typedef struct Process {
    int arrival_time;
    int size;
    int burst_time;
    int pid;
} Process;

typedef struct RunningProcess {
    int start;
    int size;
    int finish_time;
    struct RunningProcess *next;
} RunningProcess;

typedef struct WaitingProcess {
    Process process;
    int attempts;
    struct WaitingProcess *next;
} WaitingProcess;

MemoryBlock* initialize_memory(int size) {
    MemoryBlock *block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
    block->start = 0;
    block->size = size;
    block->status = FREE;
    block->next = NULL;
    return block;
}

void print_memory(MemoryBlock *head) {
    printf("\nCurrent Memory Map:\n");
    printf("+----------------+-----------+----------+\n");
    printf("| Start Address  |   Size    |  Status  |\n");
    printf("+----------------+-----------+----------+\n");
    
    MemoryBlock *current = head;
    while (current != NULL) {
        printf("| %14d | %9d | %-8s |\n", 
               current->start, 
               current->size,
               current->status == ALLOCATED ? "Alloc" : "Free");
        current = current->next;
    }
    printf("+----------------+-----------+----------+\n");
}

int calculate_total_free(MemoryBlock *head) {
    int total = 0;
    MemoryBlock *current = head;
    while (current != NULL) {
        if (current->status == FREE) total += current->size;
        current = current->next;
    }
    return total;
}

void compact_memory(MemoryBlock **head, int mem_size, RunningProcess **running) {
    printf("\n[COMPACT] Starting memory compaction...");
    
    MemoryBlock *alloc_head = NULL, *alloc_tail = NULL;
    MemoryBlock *current = *head;
    int total_allocated = 0;
    
    // Collect allocated blocks
    while (current != NULL) {
        if (current->status == ALLOCATED) {
            MemoryBlock *new_block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
            new_block->start = current->start;
            new_block->size = current->size;
            new_block->status = ALLOCATED;
            new_block->next = NULL;
            
            if (!alloc_head) alloc_head = alloc_tail = new_block;
            else {
                alloc_tail->next = new_block;
                alloc_tail = new_block;
            }
            total_allocated += new_block->size;
        }
        current = current->next;
    }
    
    // Rebuild memory layout
    MemoryBlock *new_head = NULL, *new_tail = NULL;
    int curr_start = 0;
    
    current = alloc_head;
    while (current != NULL) {
        MemoryBlock *compact_block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
        compact_block->start = curr_start;
        compact_block->size = current->size;
        compact_block->status = ALLOCATED;
        compact_block->next = NULL;

        // Update running processes
        RunningProcess *rp = *running;
        while (rp != NULL) {
            if (rp->start == current->start) {
                rp->start = curr_start;
                break;
            }
            rp = rp->next;
        }
        
        curr_start += current->size;
        
        if (!new_head) new_head = new_tail = compact_block;
        else {
            new_tail->next = compact_block;
            new_tail = compact_block;
        }
        
        current = current->next;
    }
    
    // Add remaining free space
    if (curr_start < mem_size) {
        MemoryBlock *free_block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
        free_block->start = curr_start;
        free_block->size = mem_size - curr_start;
        free_block->status = FREE;
        free_block->next = NULL;
        
        if (!new_head) new_head = free_block;
        else new_tail->next = free_block;
    }
    
    // Cleanup old memory blocks
    while (*head != NULL) {
        MemoryBlock *temp = *head;
        *head = (*head)->next;
        free(temp);
    }
    
    // Cleanup temporary list
    while (alloc_head != NULL) {
        MemoryBlock *temp = alloc_head;
        alloc_head = alloc_head->next;
        free(temp);
    }
    
    *head = new_head;
    printf("\n[COMPACT] Compaction completed. New memory layout:");
    print_memory(*head);
}

int allocate_first_fit(MemoryBlock **head, int size, int *start_addr) {
    MemoryBlock *current = *head;
    while (current != NULL) {
        if (current->status == FREE && current->size >= size) {
            if (current->size > size) {
                MemoryBlock *new_block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
                new_block->start = current->start + size;
                new_block->size = current->size - size;
                new_block->status = FREE;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            current->status = ALLOCATED;
            *start_addr = current->start;
            return 1;
        }
        current = current->next;
    }
    return 0;
}

void merge_free_blocks(MemoryBlock **head) {
    MemoryBlock *current = *head;
    while (current != NULL && current->next != NULL) {
        if (current->status == FREE && current->next->status == FREE) {
            MemoryBlock *to_merge = current->next;
            current->size += to_merge->size;
            current->next = to_merge->next;
            free(to_merge);
        } else {
            current = current->next;
        }
    }
}

void deallocate(MemoryBlock **head, int start_addr) {
    MemoryBlock *current = *head;
    while (current != NULL) {
        if (current->start == start_addr && current->status == ALLOCATED) {
            current->status = FREE;
            merge_free_blocks(head);
            return;
        }
        current = current->next;
    }
}

Process* read_process_file(const char *filename, int *count) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening file");
        return NULL;
    }
    
    Process *processes = malloc(50 * sizeof(Process));
    int idx = 0;
    char line[100];
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d %d %d", 
                   &processes[idx].arrival_time,
                   &processes[idx].size,
                   &processes[idx].burst_time) == 3) {
            processes[idx].pid = idx + 1;
            idx++;
        }
    }
    
    fclose(fp);
    *count = idx;
    return processes;
}

void add_running_process(RunningProcess **head, int start, int size, int finish) {
    RunningProcess *new_node = malloc(sizeof(RunningProcess));
    new_node->start = start;
    new_node->size = size;
    new_node->finish_time = finish;
    new_node->next = *head;
    *head = new_node;
}

void add_to_waiting(WaitingProcess **head, Process p) {
    WaitingProcess *new_node = malloc(sizeof(WaitingProcess));
    new_node->process = p;
    new_node->attempts = 0;
    new_node->next = NULL;
    
    printf("\n[QUEUE] Process %d (size %d) added to waiting queue", p.pid, p.size);
    
    if (*head == NULL) {
        *head = new_node;
    } else {
        WaitingProcess *temp = *head;
        while (temp->next != NULL) temp = temp->next;
        temp->next = new_node;
    }
}

void check_completed_processes(RunningProcess **head, MemoryBlock **memory, int current_time) {
    RunningProcess *curr = *head, *prev = NULL;
    
    while (curr != NULL) {
        if (curr->finish_time <= current_time) {
            printf("\nProcess %d at %d (size %d) completed", curr->size, curr->start, curr->size);
            deallocate(memory, curr->start);
            
            if (prev == NULL) {
                *head = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            curr = prev ? prev->next : *head;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }
}

void retry_waiting(WaitingProcess **waiting, MemoryBlock **memory, 
                  RunningProcess **running, int current_time, int mem_size,
                  int *alloc_count) {
    WaitingProcess *curr = *waiting, *prev = NULL;
    
    while (curr != NULL) {
        int start_addr;
        int max_attempts = 3;
        int allocated = 0;
        
        printf("\n[QUEUE] Trying process %d (size %d, attempts: %d)", 
              curr->process.pid, curr->process.size, curr->attempts);
        
        for (int attempt = 0; attempt < max_attempts && !allocated; attempt++) {
            int free_mem = calculate_total_free(*memory);
            
            if (free_mem < curr->process.size) {
                printf("\n[QUEUE] Insufficient total memory (%d < %d)", free_mem, curr->process.size);
                break;
            }
            
            if (allocate_first_fit(memory, curr->process.size, &start_addr)) {
                printf("\n[QUEUE] Allocation successful after %d attempts", curr->attempts + 1);
                printf("\nAllocated process %d at %d", curr->process.pid, start_addr);
                add_running_process(running, start_addr, curr->process.size,
                                   current_time + curr->process.burst_time);
                (*alloc_count)++;
                allocated = 1;
                break;
            }
            
            if (free_mem >= curr->process.size) {
                printf("\n[COMPACT] Fragmentation detected. Attempt %d", attempt+1);
                compact_memory(memory, mem_size, running);
            }
        }
        
        if (allocated) {
            // Remove from waiting queue
            if (prev == NULL) {
                *waiting = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            curr = prev ? prev->next : *waiting;
        } else {
            curr->attempts++;
            prev = curr;
            curr = curr->next;
        }
    }
}

void print_stats(Process *processes, int count, int allocated) {
    int total_size = 0;
    int max_size = 0;
    int min_size = INT_MAX;
    
    for (int i = 0; i < count; i++) {
        total_size += processes[i].size;
        if (processes[i].size > max_size) max_size = processes[i].size;
        if (processes[i].size < min_size) min_size = processes[i].size;
    }
    
    printf("\n\nFinal Statistics:");
    printf("\n+----------------------+----------+");
    printf("\n| Metric               | Value    |");
    printf("\n+----------------------+----------+");
    printf("\n| Total Processes      | %8d |", count);
    printf("\n| Successfully Alloc'd | %8d |", allocated);
    printf("\n| Average Process Size | %8.2f |", (float)total_size/count);
    printf("\n| Largest Process      | %8d |", max_size);
    printf("\n| Smallest Process     | %8d |", min_size);
    printf("\n+----------------------+----------+\n");
}

int compare_processes(const void *a, const void *b) {
    return ((Process*)a)->arrival_time - ((Process*)b)->arrival_time;
}

int main() {
    int mem_size;
    char filename[100];
    
    printf("Memory Management Module\n");
    printf("Enter total RAM size: ");
    scanf("%d", &mem_size);
    printf("Enter process file: ");
    scanf("%s", filename);
    
    MemoryBlock *memory = initialize_memory(mem_size);
    int process_count;
    Process *processes = read_process_file(filename, &process_count);
    
    if (!processes || process_count < 10) {
        printf("Error: Need at least 10 processes\n");
        free(memory);
        return 1;
    }
    
    qsort(processes, process_count, sizeof(Process), compare_processes);
    
    RunningProcess *active = NULL;
    WaitingProcess *waiting = NULL;
    int current_time = 0;
    int alloc_count = 0;
    int process_idx = 0;
    
    printf("\nSimulation Start (Memory: %d KB)\n", mem_size);
    
    while (process_idx < process_count || active != NULL || waiting != NULL) {
        printf("\nTime %d:", current_time);
        
        check_completed_processes(&active, &memory, current_time);
        
        // Handle new process arrivals
        while (process_idx < process_count && 
               processes[process_idx].arrival_time <= current_time) {
            Process p = processes[process_idx];
            
            printf("\nProcess %d arrives (size %d)", p.pid, p.size);
            
            // Reject processes larger than total memory
            if (p.size > mem_size) {
                printf("\nProcess %d size %d exceeds total memory %d. Rejected.", 
                      p.pid, p.size, mem_size);
                process_idx++;
                continue;
            }
            
            int start_addr;
            
            // Try immediate allocation first
            if (allocate_first_fit(&memory, p.size, &start_addr)) {
                printf("\nAllocated immediately at %d", start_addr);
                add_running_process(&active, start_addr, p.size, 
                                   current_time + p.burst_time);
                alloc_count++;
            } else {
                printf("\nImmediate allocation failed. Adding to queue");
                add_to_waiting(&waiting, p);
            }
            
            process_idx++;
        }
        
        // Process waiting queue
        retry_waiting(&waiting, &memory, &active, current_time, mem_size, &alloc_count);
        
        print_memory(memory);
        current_time++;
    }
    
    print_stats(processes, process_count, alloc_count);
    
    // Cleanup
    while (memory != NULL) {
        MemoryBlock *temp = memory;
        memory = memory->next;
        free(temp);
    }
    free(processes);
    
    return 0;
}
