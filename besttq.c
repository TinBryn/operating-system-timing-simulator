#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct Queue
{
    int items[MAX_PROCESSES];
    int front;
    int size;
} Queue;



int queue_size(Queue const *q);
int queue_at(Queue const *q, int i);
void queue_enqueue(Queue *q, int item);
int queue_dequeue(Queue *q);

void parse_tracefile(Tracefile *tf, char const program[], char const tracefile[]);

void print_tracefile(Tracefile const *tf);

//  ----------------------------------------------------------------------

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(Tracefile const *tf, int time_quantum, int *optimal_time_quantum, int *total_process_completion_time)
{
    (void)tf;
    *optimal_time_quantum = 0;
    *total_process_completion_time = 0;
    printf("running simulate_job_mix( time_quantum = %i usecs )\n",
           time_quantum);
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
    int total_process_completion_time = 0;

    for (int time_quantum = TQ0; time_quantum <= TQfinal; time_quantum += TQinc)
    {
        simulate_job_mix(&tf, time_quantum, &optimal_time_quantum, &total_process_completion_time);
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, total_process_completion_time);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4



enum event_type
{
    ev_io, ev_exit
};

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
    tf->processes[tf->num_processes].start_time = parse_int(start_time, "Error: process with non decimal start time (%s)");
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

#undef  MAXWORD
#undef  CHAR_COMMENT

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
    for(int i = 0; i < tf->num_devices; i++)
    {
        printf("device   %s %i bytes/sec\n", tf->devices[i].name, tf->devices[i].rate);
    }
    printf("reboot\n");
    for (int i=0; i < tf->num_processes; i++)
    {
        Process *p = &tf->processes[i];
        printf("process %i %i {\n", p->id, p->start_time);
        for (int j=0; j < p->num_events; j++)
        {
            Event *e = &p->events[j];
            switch(e->type)
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
