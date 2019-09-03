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
// DO NOT USE THIS - #define MAX_PROCESS_EVENTS      1000
#define MAX_EVENTS_PER_PROCESS    100

#define TIME_CONTEXT_SWITCH     5
#define TIME_ACQUIRE_BUS        5


//  NOTE THAT DEVICE DATA-TRANSFER-RATES ARE MEASURED IN BYTES/SECOND,
//  THAT ALL TIMES ARE MEASURED IN MICROSECONDS (usecs),
//  AND THAT THE TOTAL-PROCESS-COMPLETION-TIME WILL NOT EXCEED 2000 SECONDS
//  (SO YOU CAN SAFELY USE 'STANDARD' 32-BIT ints TO STORE TIMES).

int optimal_time_quantum = 0;
int total_process_completion_time = 0;

//  ----------------------------------------------------------------------

#define CHAR_COMMENT            '#'
#define MAXWORD                 20

typedef struct
{
    char name[MAX_DEVICE_NAME];
    //bytes / sec
    int rate;
} device;

enum event_type
{
    ev_io, ev_exit
};

typedef struct
{
    int type;
    int start_time;
    int device_index;
    int data_size;
} event;


typedef struct
{
    int id;
    int start_time;
    event events[MAX_EVENTS_PER_PROCESS];
    int num_events;
} process;

void parse_tracefile(char program[], char tracefile[])
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

    device devices[MAX_DEVICES];
    int num_devices = 0;

    process processes[MAX_PROCESSES];
    int num_processes = 0;

//  READ EACH LINE FROM THE TRACEFILE, UNTIL WE REACH THE END-OF-FILE
    while (fgets(line, sizeof line, fp) != NULL)
    {
        ++lc;

//  COMMENT LINES ARE SIMPLY SKIPPED
        if (line[0] == CHAR_COMMENT)
        {
            continue;
        }

//  ATTEMPT TO BREAK EACH LINE INTO A NUMBER OF WORDS, USING sscanf()
        char word0[MAXWORD], word1[MAXWORD], word2[MAXWORD], word3[MAXWORD];
        int nwords = sscanf(line, "%s %s %s %s", word0, word1, word2, word3);

//      printf("%i = %s", nwords, line);

//  WE WILL SIMPLY IGNORE ANY LINE WITHOUT ANY WORDS
        if (nwords <= 0)
        {
            continue;
        }
//  LOOK FOR LINES DEFINING DEVICES, PROCESSES, AND PROCESS EVENTS
        if (nwords == 4 && strcmp(word0, "device") == 0)
        {   // FOUND A DEVICE DEFINITION, WE'LL NEED TO STORE THIS SOMEWHERE
            strcpy(devices[num_devices].name, word1);
            devices[num_devices].rate = atoi(word2);
            num_devices++;
        }

        else if (nwords == 1 && strcmp(word0, "reboot") == 0)
        {   // NOTHING REALLY REQUIRED, DEVICE DEFINITIONS HAVE FINISHED

        }

        else if (nwords == 4 && strcmp(word0, "process") == 0)
        {   // FOUND THE START OF A PROCESS'S EVENTS, STORE THIS SOMEWHERE
            processes[num_processes].id = atoi(word1);
            processes[num_processes].start_time = atoi(word2);
        }

        else if (nwords == 4 && strcmp(word0, "i/o") == 0)
        {   //  AN I/O EVENT FOR THE CURRENT PROCESS, STORE THIS SOMEWHERE
            process *cur_process = processes + num_processes;
            event *cur_event = cur_process->events + cur_process->num_events;
            cur_process->num_events += 1;

            cur_event->type = ev_io;
            cur_event->start_time = atoi(word1);
            //find index of device name
            for (int i = 0; i < num_devices; i++)
            {
                if (0 == strcmp(word2, devices[i].name))
                {
                    cur_event->device_index = i;
                    break;
                }
            }
            cur_event->data_size = atoi(word3);
        }

        else if (nwords == 2 && strcmp(word0, "exit") == 0)
        {   //  PRESUMABLY THE LAST EVENT WE'LL SEE FOR THE CURRENT PROCESS
            process *cur_process = processes + num_processes;
            event *cur_event = cur_process->events + cur_process->num_events;
            cur_process->num_events += 1;

            cur_event->type = ev_exit;
            cur_event->start_time = atoi(word1);
        }

        else if (nwords == 1 && strcmp(word0, "}") == 0)
        {   //  JUST THE END OF THE CURRENT PROCESS'S EVENTS
            num_processes++;
        }
        else
        {
            printf("%s: line %i of '%s' is unrecognized",
                   program, lc, tracefile);
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);
}

#undef  MAXWORD
#undef  CHAR_COMMENT

//  ----------------------------------------------------------------------

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, FOR THE GIVEN TIME-QUANTUM
void simulate_job_mix(int time_quantum)
{
    printf("running simulate_job_mix( time_quantum = %i usecs )\n",
           time_quantum);
}

//  ----------------------------------------------------------------------

void usage(char program[])
{
    printf("Usage: %s tracefile TQ-first [TQ-final TQ-increment]\n", program);
    exit(EXIT_FAILURE);
}

int main(int argcount, char *argvalue[])
{
    int TQ0 = 0, TQfinal = 0, TQinc = 0;

//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND THREE TIME VALUES
    if (argcount == 5)
    {
        TQ0 = atoi(argvalue[2]);
        TQfinal = atoi(argvalue[3]);
        TQinc = atoi(argvalue[4]);

        if (TQ0 < 1 || TQfinal < TQ0 || TQinc < 1)
        {
            usage(argvalue[0]);
        }
    }
//  CALLED WITH THE PROVIDED TRACEFILE (NAME) AND ONE TIME VALUE
    else if (argcount == 3)
    {
        TQ0 = atoi(argvalue[2]);
        if (TQ0 < 1)
        {
            usage(argvalue[0]);
        }
        TQfinal = TQ0;
        TQinc = 1;
    }
//  CALLED INCORRECTLY, REPORT THE ERROR AND TERMINATE
    else
    {
        usage(argvalue[0]);
    }

//  READ THE JOB-MIX FROM THE TRACEFILE, STORING INFORMATION IN DATA-STRUCTURES
    parse_tracefile(argvalue[0], argvalue[1]);

//  SIMULATE THE JOB-MIX FROM THE TRACEFILE, VARYING THE TIME-QUANTUM EACH TIME.
//  WE NEED TO FIND THE BEST (SHORTEST) TOTAL-PROCESS-COMPLETION-TIME
//  ACROSS EACH OF THE TIME-QUANTA BEING CONSIDERED

    for (int time_quantum = TQ0; time_quantum <= TQfinal; time_quantum += TQinc)
    {
        simulate_job_mix(time_quantum);
    }

//  PRINT THE PROGRAM'S RESULT
    printf("best %i %i\n", optimal_time_quantum, total_process_completion_time);

    exit(EXIT_SUCCESS);
}

//  vim: ts=8 sw=4