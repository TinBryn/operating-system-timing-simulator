#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* CITS2002 Project 1 2019
   Name(s):             Kieran Griffiths, Shehjad Ibrahim
   Student number(s):   20491633, 22566957
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
#define MAX_EVENTS_PER_PROCESS  100

#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5
#define MICROSECONDS            1000000

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

#define QUEUE_SIZE MAX_PROCESSES


//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

//  ----------------------------------------------------------------------


//declare structs used

typedef struct Device Device;
typedef struct Event Event;
typedef struct Process Process;
typedef struct Tracefile Tracefile;
typedef struct ProcessSim ProcessSim;
typedef struct Queue Queue;
typedef struct SimulationState SimulationState;

//struct method prototypes

void queue_init(Queue *q);
int queue_size(Queue const *q);
int queue_at(Queue const *q, int i);
void queue_enqueue(Queue *q, int item);
int queue_dequeue(Queue *q);

void initialize_simulation(SimulationState *sim, Tracefile const *tf);

int next_process_time(SimulationState const *sim, Tracefile const *tf);
bool can_admit(SimulationState *sim, const Tracefile *tf);

//main function prototypes

int simulate_job_mix(Tracefile const *tf, int time_quantum);
void parse_tracefile(Tracefile *tf, char const program[], char const tracefile[]);
void print_tracefile(Tracefile const *tf);
void usage(char const program[]);

//struct definitions

struct Device
{
    char name[MAX_DEVICE_NAME];
    int rate; // bytes/sec
};

struct Event
{
    int type;
    int start_time;
    int device_index;
    int data_size;
};

struct Process
{
    int id;
    int start_time;
    Event events[MAX_EVENTS_PER_PROCESS];
    int num_events;
};


struct Tracefile
{
    Device devices[MAX_DEVICES];
    int num_devices;
    Process processes[MAX_PROCESSES];
    int num_processes;
};

struct ProcessSim
{
    int current_event;
    int progress;
};

struct Queue
{
    int items[QUEUE_SIZE];
    int front;
    int size;
};

struct SimulationState
{
    int time;
    Queue admit_queue;
    Queue blocked_queues[MAX_DEVICES];
    ProcessSim processes[MAX_PROCESSES];
    int next_process;
    int running_process;
    int io_process;
    int time_to_admit;
    int time_to_expire;
    int time_to_event;
    int time_to_databus;
    int time_to_dispatch;
    int device_priorities[MAX_DEVICES];
};

enum
{
    ADMIT, EXPIRE, EVENT, DATABUS, DISPATCH
};

int next_trigger(SimulationState *sim);

enum event_type
{
    ev_io, ev_exit
};


void print_state(Tracefile const *tf, int running, Queue const *queue)
{
    printf("    ( running = ");
    if (running >= 0)
        printf("p%i  RQ = [", tf->processes[running].id);
    else
        printf("__  RQ = [");

    if (queue_size(queue) > 0)
        printf("%i", tf->processes[queue_at(queue, 0)].id);

    for (int i = 1; i < queue_size(queue); i++)
    {
        printf(",%i", tf->processes[queue_at(queue, i)].id);
    }
    printf("] )\033[0m\n");
}

void print_time(SimulationState const *sim);
void print_admit(SimulationState const *sim, const Tracefile *tf);
void print_dispatch(SimulationState const *sim, Tracefile const *tf);
void print_refresh(SimulationState const *sim, const Tracefile *tf);
void print_release(SimulationState const *sim, const Tracefile *tf);
void print_expire(SimulationState const *sim, const Tracefile *tf);
void print_databus(SimulationState const *sim, const Tracefile *tf);
void print_block(SimulationState const *sim, const Tracefile *tf);

//  ----------------------------------------------------------------------

void FSM_admit(SimulationState *sim, Tracefile const *tf);
void FSM_dispatch(SimulationState *sim, Tracefile const *tf, int time_quantum);
void FSM_expire(SimulationState *sim, Tracefile const *tf, int time_quantum);
void FSM_loop(SimulationState *sim, Tracefile const *tf, int time_quantum);
void FSM_databus(SimulationState *sim, Tracefile const *tf);
void FSM_event(SimulationState *sim, Tracefile const *tf);
void FSM_release(SimulationState *sim, Tracefile const *tf);
void FSM_block(SimulationState *sim, Tracefile const *tf);
void FSM_start_io(SimulationState *sim, Tracefile const *tf);

void advance_time(SimulationState *sim, int time_delta);


//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
int simulate_job_mix(Tracefile const *tf, int time_quantum)
{
    SimulationState sim;

    initialize_simulation(&sim, tf);

    int start_time = 0;

    if (can_admit(&sim, tf))
    {
        start_time = next_process_time(&sim, tf);
    }

    printf("running simulate_job_mix( time_quantum = %i usecs )\n", time_quantum);

    printf("@00000000   [reboot]    TQ = % 4i            ", time_quantum);
    print_state(tf, sim.running_process, &sim.admit_queue);

    //The simulation starts in a loop
    FSM_loop(&sim, tf, time_quantum);
    return sim.time - start_time;
}

void FSM_loop(SimulationState *sim, const Tracefile *tf, int time_quantum)
{
    switch (next_trigger(sim))
    {
        case ADMIT:
            advance_time(sim, sim->time_to_admit);
            FSM_admit(sim, tf);
            FSM_loop(sim, tf, time_quantum);
            break;
        case EXPIRE:
            advance_time(sim, sim->time_to_expire);
            FSM_expire(sim, tf, time_quantum);
            FSM_loop(sim, tf, time_quantum);
            break;
        case EVENT:
            advance_time(sim, sim->time_to_event);
            FSM_event(sim, tf);
            FSM_loop(sim, tf, time_quantum);
            break;
        case DATABUS:
            advance_time(sim, sim->time_to_databus);
            FSM_databus(sim, tf);
            FSM_loop(sim, tf, time_quantum);
            break;
        case DISPATCH:
            advance_time(sim, sim->time_to_dispatch);
            FSM_dispatch(sim, tf, time_quantum);
            FSM_loop(sim, tf, time_quantum);
            break;
        default:
            // no more triggers so done
            break;
    }
}

void FSM_admit(SimulationState *sim, const Tracefile *tf)
{
    printf("\033[42m");
    print_time(sim);
    printf("[admit]     ");
    queue_enqueue(&sim->admit_queue, sim->next_process);
    print_admit(sim, tf);
    sim->next_process++;

    //set trigger to dispatch
    if (sim->running_process == -1 && sim->time_to_dispatch == -1)
        sim->time_to_dispatch = TIME_CONTEXT_SWITCH;

    //set trigger for next admit if it exists
    if (can_admit(sim, tf))
    {
        sim->time_to_admit = next_process_time(sim, tf);
    }
    else
    {
        sim->time_to_admit = -1;
    }
}

void FSM_expire(SimulationState *sim, Tracefile const *tf, int time_quantum)
{
    printf("\033[47m");
    print_time(sim);
    printf("[expire]    ");

    // if an io process in running make progress


    // if there is nothing waiting, just refresh and make progress
    if (queue_size(&sim->admit_queue) == 0)
    {
        print_refresh(sim, tf);
        sim->time_to_expire = time_quantum;
    }
    else
    {
        queue_enqueue(&sim->admit_queue, sim->running_process);
        sim->running_process = -1;
        print_expire(sim, tf);

        //no running process so remove triggers
        sim->time_to_event = -1;
        sim->time_to_expire = -1;

        //add trigger to dispatch
        sim->time_to_dispatch = TIME_CONTEXT_SWITCH;
    }
}

void FSM_databus(SimulationState *sim, Tracefile const *tf)
{
    print_time(sim);
    printf("[databus]   ");

    //no more io so remove process and triggers
    sim->time_to_databus = -1;
    queue_enqueue(&sim->admit_queue, sim->io_process);
    if (sim->running_process == -1)
        sim->time_to_dispatch = TIME_CONTEXT_SWITCH;
    print_databus(sim, tf);
    sim->io_process = -1;

    FSM_start_io(sim, tf);
}

void FSM_dispatch(SimulationState *sim, Tracefile const *tf, int time_quantum)
{
    printf("\033[46m");
    print_time(sim);
    printf("[dispatch]  ");

    sim->running_process = queue_dequeue(&sim->admit_queue);

    print_dispatch(sim, tf);

    int pid = sim->running_process;
    int eid = sim->processes[pid].current_event;

    sim->time_to_expire = time_quantum;
    sim->time_to_event = tf->processes[pid].events[eid].start_time - sim->processes[pid].progress;
    sim->time_to_dispatch = -1;
}

void FSM_event(SimulationState *sim, Tracefile const *tf)
{
    advance_time(sim, sim->time_to_event);


    int pid = sim->running_process;
    int eid = sim->processes[pid].current_event;
    switch (tf->processes[pid].events[eid].type)
    {
        case ev_exit:
            FSM_release(sim, tf);
            sim->processes[pid].current_event++;
            break;
        case ev_io:
            FSM_block(sim, tf);
            break;
        default:
            break;
    }
}

void FSM_release(SimulationState *sim, Tracefile const *tf)
{
    printf("\033[41m");
    print_time(sim);
    printf("[release]   ");
    print_release(sim, tf);

    sim->running_process = -1;
    //set triggers
    if (queue_size(&sim->admit_queue) != 0)
        sim->time_to_dispatch = TIME_CONTEXT_SWITCH;
    else
        sim->time_to_dispatch = -1;
    sim->time_to_expire = -1;
    sim->time_to_event = -1;
}

void FSM_block(SimulationState *sim, Tracefile const *tf)
{
    printf("\033[43m");
    print_time(sim);
    printf("[block]     ");
    print_block(sim, tf);

    int pid = sim->running_process;
    int eid = sim->processes[pid].current_event;

    //get the requested device and add it to the queue
    int did = tf->processes[pid].events[eid].device_index;
    queue_enqueue(&sim->blocked_queues[did], pid);
    sim->running_process = -1;
    sim->time_to_expire = -1;
    sim->time_to_event = -1;

    if (sim->io_process == -1)
    {
        FSM_start_io(sim, tf);

    }
    if (queue_size(&sim->admit_queue) != 0)
        sim->time_to_dispatch = TIME_CONTEXT_SWITCH;
}

void FSM_start_io(SimulationState *sim, Tracefile const *tf)
{
    for (int i = 0; i < tf->num_devices; i++)
    {
        int device = sim->device_priorities[i];
        if (queue_size(&sim->blocked_queues[device]) != 0)
        {
            sim->io_process = queue_dequeue(&sim->blocked_queues[device]);

            int pid = sim->io_process;
            int eid = sim->processes[pid].current_event;

            long data = (long) tf->processes[pid].events[eid].data_size * MICROSECONDS;
            int rate = tf->devices[device].rate;

            long time = data / rate;
            if (data % rate != 0)
                time += 1;
            sim->time_to_databus = time + TIME_ACQUIRE_BUS;

            sim->processes[pid].current_event++;
            break;
        }
    }
}

int next_trigger(SimulationState *sim)
{
    int min = -1;

    if (sim->time_to_admit != -1)
        min = sim->time_to_admit;

    if (sim->time_to_databus != -1 && (sim->time_to_databus < min || min == -1))
        min = sim->time_to_databus;

    if (sim->time_to_expire != -1 && (sim->time_to_expire < min || min == -1))
        min = sim->time_to_expire;

    if (sim->time_to_event != -1 && (sim->time_to_event < min || min == -1))
        min = sim->time_to_event;

    if (sim->time_to_dispatch != -1 && (sim->time_to_dispatch < min || min == -1))
        min = sim->time_to_dispatch;

    if (min == -1)
        return -1;

    if (min == sim->time_to_admit)
        return ADMIT;
    if (min == sim->time_to_event)
        return EVENT;
    if (min == sim->time_to_expire)
        return EXPIRE;
    if (min == sim->time_to_databus)
        return DATABUS;
    return DISPATCH;
}

void advance_time(SimulationState *sim, int time_delta)
{
    sim->time += time_delta;

    if (sim->io_process != -1)
        sim->time_to_databus -= time_delta;

    if (sim->time_to_dispatch != -1)
        sim->time_to_dispatch -= time_delta;

    if (sim->time_to_admit != -1)
        sim->time_to_admit -= time_delta;

    if (sim->running_process != -1)
    {
        sim->time_to_event -= time_delta;
        sim->time_to_expire -= time_delta;
        sim->processes[sim->running_process].progress += time_delta;
    }
}


bool can_admit(SimulationState *sim, const Tracefile *tf)
{
    return sim->next_process < tf->num_processes;
}

//  ----------------------------------------------------------------------

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
    sim->next_process = 0;                      //start on process index 0
    sim->time = 0;                              //start at time 0

    //no triggers except for admit
    sim->time_to_databus = -1;
    sim->time_to_event = -1;
    sim->time_to_expire = -1;
    sim->time_to_dispatch = -1;

    //if there is a process prepare to admit
    if (can_admit(sim, tf))
        sim->time_to_admit = next_process_time(sim, tf);

    //no process is running
    sim->running_process = -1;
    sim->io_process = -1;

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

    //sort the device priorities

    for (int i = 0; i < tf->num_devices; i++)
    {
        sim->device_priorities[i] = i;
    }

    for (int j = 1; j < tf->num_devices; j++)
    {
        int key = sim->device_priorities[j];
        int i = j;
        while (i > 0 && tf->devices[sim->device_priorities[i - 1]].rate < tf->devices[key].rate)
        {
            sim->device_priorities[i] = sim->device_priorities[i - 1];
            i--;
        }
        sim->device_priorities[i] = key;
    }

    for (int i = 0; i < tf->num_devices; i++)
    {
        printf("rate %i: %i\n", i, tf->devices[sim->device_priorities[i]].rate);
    }
}

int next_process_time(SimulationState const *sim, Tracefile const *tf)
{
    return tf->processes[sim->next_process].start_time - sim->time;
}

void print_admit(SimulationState const *sim, const Tracefile *tf)
{
    printf("p%i.NEW -> READY      ", tf->processes[sim->next_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}

void print_dispatch(SimulationState const *sim, Tracefile const *tf)
{
    printf("p%i.READY -> RUNNING  ", tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}

void print_refresh(SimulationState const *sim, const Tracefile *tf)
{
    printf("p%i.refreshTQ         ", tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}
void print_release(SimulationState const *sim, const Tracefile *tf)
{
    printf("p%i.RUNNING->EXIT     ", tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}
void print_expire(SimulationState const *sim, const Tracefile *tf)
{
    printf("p%i.RUNNING -> READY  ", tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}
void print_databus(SimulationState const *sim, const Tracefile *tf)
{
    int pid = tf->processes[sim->io_process].id;
    printf("p%i.release_databus, p%i.BLOCKED -> READY  ", pid, pid);
    print_state(tf, sim->running_process, &sim->admit_queue);
}
void print_block(SimulationState const *sim, const Tracefile *tf)
{
    printf("p%i.RUNNING -> BLOCKED", tf->processes[sim->running_process].id);
    print_state(tf, sim->running_process, &sim->admit_queue);
}
void print_time(SimulationState const *sim)
{
    printf("@%08i   ", sim->time);
}
