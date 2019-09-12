#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* CITS2002 Project 1 2019
   Name(s):             student-name1 (, student-name2)
   Student number(s):   student-number-1 (, student-number-2)
 */


//  besttq (v1.0)
//  Written by Chris.McDonald@uwa.edu.au, 2019, free for all to copy and modify

//  Compile with:  cc -std=c99 -Wall -Werror -o besttq besttq.c


//  THESE CONSTANTS DEFINE THE MAXIMUM SIZE OF TRACEFILE CONTENTS (AND HENCE
//  JOB-MIX) THAT YOUR PROGRAM NEEDS TO SUPPORT.  YOU'LL REQUIRE THESE
//  CONSTANTS WHEN DEFINING THE MAXIMUM SIZES OF ANY REQUIRED DATA STRUCTURES.

#define MAX_DEVICES             4
#define MAX_DEVICE_NAME         20
#define MAX_PROCESSES           50
#define MAX_EVENTS_PER_PROCESS    100

#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5


//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

//  ----------------------------------------------------------------------

typedef struct Device
{
    char name[MAX_DEVICE_NAME];
    int rate; // bytes/sec
} Device;

typedef struct Event
{
    int type;
    int start_time;
    int device_index;
    int data_size;
} Event;

typedef struct Process
{
    int id;
    int start_time;
    Event events[MAX_EVENTS_PER_PROCESS];
    int num_events;
} Process;


typedef struct Tracefile
{
    Device devices[MAX_DEVICES];
    int num_devices;
    Process processes[MAX_PROCESSES];
    int num_processes;
} Tracefile;

typedef struct ProcessSim
{
    int current_event;
    int progress;
} ProcessSim;

int min_index3(int a, int b, int c);
enum event_type
{
    ev_io, ev_exit
};

#define QUEUE_SIZE MAX_PROCESSES

typedef struct Queue
{
    int items[QUEUE_SIZE];
    int front;
    int size;
} Queue;

void queue_init(Queue *q);
int queue_size(Queue const *q);
int queue_at(Queue const *q, int i);
void queue_enqueue(Queue *q, int item);
int queue_dequeue(Queue *q);

void parse_tracefile(Tracefile *tf, char const program[], char const tracefile[]);

void print_tracefile(Tracefile const *tf);

void print_state(Tracefile const *tf, int running, Queue const *queue)
{
    printf("    (running=");
    if (running >= 0)
        printf("p%i  RQ=[", tf->processes[running].id);
    else
        printf("__  RQ=[");

    if (queue_size(queue) > 0)
        printf("%i", tf->processes[queue_at(queue, 0)].id);

    for (int i = 1; i < queue_size(queue); i++)
    {
        printf(",%i", tf->processes[queue_at(queue, i)].id);
    }
    printf("])\n");
}


typedef struct SimulationState
{
    int time;
    Queue admit_queue;
    Queue blocked_queues[MAX_DEVICES];
    ProcessSim processes[MAX_PROCESSES];
    int next_event;
    int next_process;
    int running_process;
} SimulationState;

void initialize_simulation(SimulationState *sim, Tracefile const *tf);

void print_admit(SimulationState const *sim, const Tracefile *tf);
void print_dispatch(SimulationState const *sim, Tracefile const *tf);
void print_refresh(SimulationState const *sim, const Tracefile *tf);
void print_release(SimulationState const *sim, const Tracefile *tf);
void print_expire(SimulationState const *sim, const Tracefile *tf);

//  ----------------------------------------------------------------------

int next_process_time(SimulationState const *sim, Tracefile const *tf);
bool can_admit(SimulationState *sim, const Tracefile *tf);

void admit_handler(SimulationState *sim, const Tracefile *tf);
void not_running_handler(SimulationState *sim, const Tracefile *tf);
void dispatch_handler(SimulationState *sim, const Tracefile *tf);
void io_handler(SimulationState *sim, const Tracefile *tf);
void running_handler(SimulationState *sim, const Tracefile *tf, int time_quantum);
void try_admit_running_handler(SimulationState *sim, const Tracefile *tf, int time_quantum);
void admit_running_handler(SimulationState *sim, const Tracefile *tf);
void event_handler(SimulationState *sim, const Tracefile *tf);
void release_handler(SimulationState *sim, Tracefile const *tf);
void block_handler(SimulationState *sim, const Tracefile *tf);
void expire_handler(SimulationState *sim, const Tracefile *tf);
//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
int simulate_job_mix(Tracefile const *tf, int time_quantum)
{
    SimulationState sim;

    initialize_simulation(&sim, tf);

    int start_time = sim.time;

    printf("running simulate_job_mix( time_quantum = %i usecs )\n", time_quantum);

    #define ADMIT_NOT_RUNNING 0
    #define NOT_RUNNING 1
    #define DISPATCH 2
    #define IO 3
    #define RUNNING 4
    #define TRY_ADMIT_RUNNING 5
    #define ADMIT_RUNNING 6
    #define EVENT 7
    #define RELEASE 8
    #define BLOCK 9
    #define EXPIRE 10
    #define DONE -1

    printf("@0000000  reboot with TQ=%i\n", time_quantum);
    print_state(tf, sim.running_process, &sim.admit_queue);

    while (sim.next_event != DONE)
    {
        switch (sim.next_event)
        {
            case ADMIT_NOT_RUNNING:
                admit_handler(&sim, tf);
                break;

            case NOT_RUNNING:
                not_running_handler(&sim, tf);
                break;

            case DISPATCH:
                dispatch_handler(&sim, tf);
                break;

            case IO:
                io_handler(&sim, tf);
                break;

            case RUNNING:
                running_handler(&sim, tf, time_quantum);
                break;

            case TRY_ADMIT_RUNNING:
                try_admit_running_handler(&sim, tf, time_quantum);
                break;

            case ADMIT_RUNNING:
                admit_running_handler(&sim, tf);
                break;

            case EVENT:
                event_handler(&sim, tf);
                break;

            case RELEASE:
                release_handler(&sim, tf);
                break;

            case BLOCK:
                block_handler(&sim, tf);
                break;

            case EXPIRE:
                expire_handler(&sim, tf);
                break;
            default:
                break;
        }
    }


    return sim.time - start_time;
}
void admit_handler(SimulationState *sim, const Tracefile *tf)
{
    printf("admit\n");
    if (can_admit(sim, tf))
    {
        queue_enqueue(&sim->admit_queue, sim->next_process);
        print_admit(sim, tf);
        sim->next_process++;
    }
    sim->next_event = NOT_RUNNING;
}
void not_running_handler(SimulationState *sim, const Tracefile *tf)
{
    printf("not running\n");
    //todo: branch to IO events
    if (can_admit(sim, tf))
    {
        int time_admit = next_process_time(sim, tf);
        if (time_admit < TIME_CONTEXT_SWITCH)   //can admit before dispatch
        {
            sim->next_event = ADMIT_NOT_RUNNING;
            sim->time += time_admit;
            return;
        }
        if (queue_size(&sim->admit_queue) != 0) //can dispatch
        {
            sim->next_event = DISPATCH;
            sim->time += TIME_CONTEXT_SWITCH;
            return;
        }

        //nothing to dispatch so admit next process
        sim->next_event = ADMIT_NOT_RUNNING;
        sim->time += time_admit;
        return;
    }

    if (queue_size(&sim->admit_queue) != 0)
    {
        sim->next_event = DISPATCH;
        sim->time += TIME_CONTEXT_SWITCH;
        return;
    }

    //nothing can run, nothing to admit
    sim->next_event = DONE;
}
void dispatch_handler(SimulationState *sim, const Tracefile *tf)
{
    printf("dispatch\n");
    sim->running_process = queue_dequeue(&sim->admit_queue);
    print_dispatch(sim, tf);
    sim->next_event = RUNNING;
}
void io_handler(SimulationState *sim, const Tracefile *tf)
{
    printf("io\n");
    //todo: implement io
    sim->next_event = NOT_RUNNING;
}
void running_handler(SimulationState *sim, const Tracefile *tf, int time_quantum)
{
    printf("running\n");
    if (can_admit(sim, tf))
    {
        sim->next_event = TRY_ADMIT_RUNNING;
        return;
    }
    int pid = sim->running_process;
    int eid = sim->processes[pid].current_event;
    int time_event = tf->processes[pid].events[eid].start_time - sim->processes[pid].progress;
    if (time_quantum < time_event)
    {
        sim->next_event = EXPIRE;
        sim->time += time_quantum;
        sim->processes[pid].progress += time_quantum;
    }
    else
    {
        sim->next_event = EVENT;
        sim->time += time_event;
        sim->processes[pid].progress += time_event;
    }
}
void try_admit_running_handler(SimulationState *sim, const Tracefile *tf, int time_quantum)
{
    int time_admit = tf->processes[sim->next_process].start_time - sim->time;
    int pid = sim->running_process;
    int eid = sim->processes[pid].current_event;
    int time_event = tf->processes[pid].events[eid].start_time - sim->processes[pid].progress;

    switch (min_index3(time_admit, time_event, time_quantum))
    {
        case 0:
            sim->next_event = ADMIT_RUNNING;
            sim->time += time_admit;
            sim->processes[pid].progress += time_admit;
            break;
        case 1:
            sim->next_event = EVENT;
            sim->time += time_event;
            sim->processes[pid].progress += time_event;
            break;
        case 2:
            sim->next_event = EXPIRE;
            sim->time += time_quantum;
            sim->processes[pid].progress += time_quantum;
    }
}
void admit_running_handler(SimulationState *sim, const Tracefile *tf)
{
    printf("admit running\n");
    queue_enqueue(&sim->admit_queue, sim->next_process);
    print_admit(sim, tf);
    sim->next_event = RUNNING;
    sim->next_process++;
}
void event_handler(SimulationState *sim, const Tracefile *tf)
{
    int pid = sim->running_process;
    int eid = sim->processes[pid].current_event;
    if (tf->processes[pid].events[eid].type == ev_exit)
    {
        sim->next_event = RELEASE;
    }
    else
    {
        sim->next_event = BLOCK;
    }
}
void release_handler(SimulationState *sim, Tracefile const *tf)
{
    printf("release\n");
    print_release(sim, tf);
    sim->running_process = -1;
    sim->next_event = NOT_RUNNING;
}
void block_handler(SimulationState *sim, const Tracefile *tf)
{

}
void expire_handler(SimulationState *sim, const Tracefile *tf)
{
    printf("expire\n");
    if (queue_size(&sim->admit_queue) == 0)
    {
        print_refresh(sim, tf);
        sim->next_event = RUNNING;
    }
    else
    {
        print_expire(sim, tf);
        sim->running_process = -1;
        sim->next_event = NOT_RUNNING;
    }
}

int min_index3(int a, int b, int c)
{
    int min = a;
    if (b < min)
        min = b;
    if (c < min)
        min = c;
    if (a == min)
        return 0;
    if (b == min)
        return 1;
    return 2;
}
bool can_admit(SimulationState *sim, const Tracefile *tf)
{
    return sim->next_process < tf->num_processes;
}
//  ----------------------------------------------------------------------

void usage(char const program[]);

/**
 * Attempts to parse a string into an integer, if it fails, reports an error and exits the program
 * @param str - The string to be parsed
 * @param err - The error message, must be formatted to accept a single string
 * @return the parsed integer if successful
 */
int parse_int(char const *str, char const *err)
{
    char *ptr;
    int result = strtol(str, &ptr, 10);
    if (str == ptr)
    {
        printf(err, str);
        exit(EXIT_FAILURE);
    }
    return result;
}

int main(int argc, char const *argv[])
{
    int TQ0 = 0, TQfinal = 0, TQinc = 0;

//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND THREE TIME VALUES
    if (argc == 5)
    {
        TQ0 = parse_int(argv[2], "Error: non decimal initial time quantum (%s)");
        TQfinal = parse_int(argv[3], "Error: non decimal final time quantum (%s)");
        TQinc = parse_int(argv[4], "Error: non decimal time quantum increment (%s)");

        if (TQ0 < 1 || TQfinal < TQ0 || TQinc < 1)
        {
            usage(argv[0]);
        }
    }
//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND ONE TIME VALUE
    else if (argc == 3)
    {
        TQ0 = parse_int(argv[2], "Error: non decimal initial time quantum (%s)");

        if (TQ0 < 1)
        {
            usage(argv[0]);
        }
        TQfinal = TQ0;
        TQinc = 1;
    }
//  CALLED INCORRECTLY, REPORT THE ERROR AND TERMINATE
    else
    {
        usage(argv[0]);
    }

//  READ THE JOB-MIX FROM THE TRACEFILE, STORING INFORMATION IN DATA-STRUCTURES
    Tracefile tf = {};

    parse_tracefile(&tf, argv[0], argv[1]);

    print_tracefile(&tf);

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, VARYING THE TIME-QUANTUM EACH TIME.
//  WE NEED TO FIND THE BEST (SHORTEST) TOTAL-PROCESS-COMPLETION-TIME
//  ACROSS EACH OF THE TIME-QUANTA BEING CONSIDERED

    int optimal_time_quantum = 0;
    int best_total_completion_time = -1;

    for (int time_quantum = TQ0; time_quantum <= TQfinal; time_quantum += TQinc)
    {
        int current_process_completion_time = simulate_job_mix(&tf, time_quantum);
        if (best_total_completion_time == -1 || current_process_completion_time <= best_total_completion_time)
        {
            best_total_completion_time = current_process_completion_time;
            optimal_time_quantum = time_quantum;
        }
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, best_total_completion_time);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4


void push_io_event_detail(Event *e, Device devices[], int num_devices, char const *start_time, char const *device,
                          char const *data)
{
    e->type = ev_io;
    for (int i = 0; i < num_devices; i++)
    {
        if (0 == strcmp(device, devices[i].name))
        {
            e->device_index = i;
            break;
        }
    }

    e->start_time = parse_int(start_time, "Error: io event with non decimal start time (%s)");
    e->data_size = parse_int(data, "Error: io event with non decimal data size (%s)");

}
void push_io_event(Tracefile *tf, char const *start_time, char const *device, char const *data)
{

    Process *p = tf->processes + tf->num_processes;
    Event *e = p->events + p->num_events;
    push_io_event_detail(e, tf->devices, tf->num_devices, start_time, device, data);

    p->num_events++;
}
void push_exit_event_detail(Event *e, char const *start_time)
{
    e->type = ev_exit;
    e->start_time = parse_int(start_time, "Error: exit event with non decimal start time (%s)");

}
void push_exit_event(Tracefile *tf, char const *start_time)
{
    Process *p = tf->processes + tf->num_processes;
    Event *e = p->events + p->num_events;
    push_exit_event_detail(e, start_time);

    p->num_events++;
}
void push_device(Tracefile *tf, char const *name, char const *rate)
{
    tf->devices[tf->num_devices].rate = parse_int(rate, "Error: device with non decimal rate (%s)");
    strcpy(tf->devices[tf->num_devices].name, name);
    tf->num_devices++;
}
void push_process(Tracefile *tf, char const *id, char const *start_time)
{
    tf->processes[tf->num_processes].id = parse_int(id, "Error: process with non decimal id (%s)");
    tf->processes[tf->num_processes].start_time = parse_int(start_time,
                                                            "Error: process with non decimal start time (%s)");
}

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

void parse_line(Tracefile *tf, char const *program, char const *tracefile, char const *line, int line_num)
{
    //  COMMENT LINES ARE SIMPLY SKIPPED
    if (line[0] == CHAR_COMMENT)
    {
        return;
    }

    //  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
    char word[4][MAXWORD];
    int nwords = sscanf(line, "%s %s %s %s", word[0], word[1], word[2], word[3]);

    //  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
    if (nwords <= 0)
    {
        return;
    }

    //  LOOK FOR LINES DEFINING DEVICES, PROCESSES, AND PROCESS EVENTS
    if (nwords == 4 && strcmp(word[0], "device") == 0)
    {
        // device <name> <rate> bytes/sec
        push_device(tf, word[1], word[2]);
    }

    else if (nwords == 4 && strcmp(word[0], "process") == 0)
    {
        // process <id> <start_time> {
        push_process(tf, word[1], word[2]);
    }

    else if (nwords == 4 && strcmp(word[0], "i/o") == 0)
    {
        // i/o <start_time> <device_name> <data_size>
        push_io_event(tf, word[1], word[2], word[3]);
    }

    else if (nwords == 2 && strcmp(word[0], "exit") == 0)
    {
        // exit <start_time>
        push_exit_event(tf, word[1]);
    }

    else if (nwords == 1 && strcmp(word[0], "}") == 0)
    {
        // JUST THE END OF THE CURRENT PROCESS'S EVENTS
        tf->num_processes++;
    }

    else if (nwords == 1 && strcmp(word[0], "reboot") == 0)
    {
        // NOTHING REALLY REQUIRED, DEVICE DEFINITIONS HAVE FINISHED
    }

    else
    {
        printf("%s: line %i of '%s' is unrecognized",
               program, line_num, tracefile);
        exit(EXIT_FAILURE);
    }
}

void parse_tracefile(Tracefile *tf, char const *program, char const *tracefile)
{
//  ATTEMPT TO OPEN OUR TRACEFILE, REPORTING AN ERROR IF WE CAN'T
    FILE *fp = fopen(tracefile, "r");

    if (fp == NULL)
    {
        printf("%s: unable to open '%s'\n", program, tracefile);
        exit(EXIT_FAILURE);
    }

    char line[BUFSIZ];
    int lc = 0;

//  READ EACH LINE FROM THE TRACEFILE, UNTIL WE REACH THE END-OF-FILE
    while (fgets(line, sizeof line, fp) != NULL)
    {
        ++lc;

        parse_line(tf, program, tracefile, line, lc);
    }
    fclose(fp);
}
void usage(char const *program)
{
    printf("Usage: %s tracefile TQ-first [TQ-final TQ-increment]\n", program);
    exit(EXIT_FAILURE);
}

void print_tracefile(Tracefile const *tf)
{
    for (int i = 0; i < tf->num_devices; i++)
    {
        printf("device   %s %i bytes/sec\n", tf->devices[i].name, tf->devices[i].rate);
    }
    printf("reboot\n");
    for (int i = 0; i < tf->num_processes; i++)
    {
        Process const *p = &tf->processes[i];
        printf("process %i %i {\n", p->id, p->start_time);
        for (int j = 0; j < p->num_events; j++)
        {
            Event const *e = &p->events[j];
            switch (e->type)
            {
                case ev_io:
                    printf("  i/o ");
                    break;
                case ev_exit:
                    printf("  exit ");
                    break;
                default:
                    printf("unknown event type");
                    break;
            }
            printf("%i %s %i\n", e->start_time, tf->devices[e->device_index].name, e->data_size);
        }
        printf("}\n");
    }

}

void queue_enqueue(Queue *q, int item)
{
    //check for overflow
    if (q->size >= QUEUE_SIZE)
    {
        printf("Trying to enqueue to a full queue");
        exit(EXIT_FAILURE);
    }
    int back = (q->front + q->size) % QUEUE_SIZE;
    q->items[back] = item;
    q->size++;
}
int queue_size(Queue const *q)
{
    return q->size;
}
int queue_at(Queue const *q, int i)
{
    //check for out of bounds
    if (i < 0 || i >= q->size)
    {
        printf("Queue index out of bounds (%i) should be in [0, %i)", i, q->size);
        exit(EXIT_FAILURE);
    }

    //find where to look in the array
    int index = (q->front + i) % QUEUE_SIZE;

    return q->items[index];
}
int queue_dequeue(Queue *q)
{
    //check for underflow
    if (q->size <= 0)
    {
        printf("Trying to dequeue from an empty queue");
        exit(EXIT_FAILURE);
    }
    //retrieve the item
    int item = queue_at(q, 0);

    //decrement the size
    q->size--;

    //move the front of the queue back one
    q->front = (q->front + 1) % QUEUE_SIZE;
    return item;
}
void queue_init(Queue *q)
{
    q->size = 0;
    q->front = 0;
}

void initialize_simulation(SimulationState *sim, Tracefile const *tf)
{
    if (tf->num_processes == 0)
        sim->next_event = DONE;                 //no processes so nothing to do

    sim->next_event = ADMIT_NOT_RUNNING;        //admit the first process
    sim->next_process = 0;                      //start on process index 0
    sim->time = tf->processes[0].start_time;    //start when the first process starts
    sim->running_process = -1;                  //no process is running

    //initialise queues
    queue_init(&sim->admit_queue);
    for (int i = 0; i < tf->num_devices; i++)
    {
        queue_init(&sim->blocked_queues[i]);
    }

    //initialize process simulations
    for (int i = 0; i < tf->num_processes; i++)
    {
        sim->processes[i].current_event = 0;
        sim->processes[i].progress = 0;
    }
}

int next_process_time(SimulationState const *sim, Tracefile const *tf)
{
    if (sim->next_process < tf->num_processes)
        return tf->processes[sim->next_process].start_time - sim->time;
    return 1000000000;
}

int next_event_time(SimulationState const *sim, Tracefile const *tf)
{
    int pid = sim->running_process;
    ProcessSim const *psim = &sim->processes[pid];
    return tf->processes[pid].events[psim->current_event].start_time - psim->progress;
}

void print_admit(SimulationState const *sim, const Tracefile *tf)
{
    printf("@%08i   p%i.NEW -> READY\n", sim->time, tf->processes[sim->next_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}

void print_dispatch(SimulationState const *sim, Tracefile const *tf)
{
    printf("@%08i   p%i.READY -> RUNNING\n", sim->time, tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}

void print_refresh(SimulationState const *sim, const Tracefile *tf)
{
    printf("@%08i   p%i.refreshTQ\n", sim->time, tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}

void print_release(SimulationState const *sim, const Tracefile *tf)
{
    printf("@%08i   p%i.RUNNING->EXIT\n", sim->time, tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}

void print_expire(SimulationState const *sim, const Tracefile *tf)
{
    printf("@%08i   p%i.RUNNING -> READY\n", sim->time, tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}
