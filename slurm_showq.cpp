//-------------------------------------------------------------------------
// Copyright (c) 2012 The Regents of the University of Texas System.
// All rights reserved.
//
// Redistribution and use in source and binary forms are permitted
// provided that the above copyright notice and this paragraph are
// duplicated in all such forms and that any documentation,
// advertising materials, and other materials related to such
// distribution and use acknowledge that the software was developed by
// the University of Texas.  The name of the University may not be
// used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
// WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
//-------------------------------------------------------------------------
// A SLURM derived showq for summarizing user jobs.
//
// Originally: 7/29/2012 (refactor of original LSF version)
//
// Karl W. Schulz- Texas Advanced Computing Center (karl@tacc.utexas.edu)
//
//
// Last update: Si Liu on 5/18/2016
// $Id: slurm_showq.cpp 2015-01-05  siliu $
//-------------------------------------------------------------------------*/ 

#include "slurm_showq.h"

void Slurm_Showq::query_running_jobs()
{
  char *user, *starttime, *shorttime, *pendreason, *endtime, *submittime;
  time_t current_time, remain_time;
  char jobuser_short  [256];
  char jobname_short  [256];
  char queuename_short[256];
  int i, j, status;

  int rjobs  = 0;
  int rprocs = 0;
  int rnodes = 0;

  int dprocs, dnodes;
  int ijobs, bjobs, error_jobs;
  int running_jobs; 
  int reserv_procs;
  int numHosts = 1;
  long hours_max, secs_max;
  long hours_remain, mins_remain, secs_remain;

  int num_pending_jobs    = 0;
  int num_completing_jobs = 0;
  int idle_jobs    = 0;
  int blocked_jobs = 0;
  int num_closed   = 0;
  char queuename[8];
  char current_user[256];
  int num_nodes;
  long int time1, time2;

  //  const int total_avail_procs = 5760*16;

  std::vector<uint32_t> completing_jobs; // list of jobs in completing state
  std::vector<uint32_t> pending_jobs;	 // list of jobs in pending state
  std::vector<uint32_t> errored_jobs;	 // list of jobs with a problem

  typedef struct koomie_job_struct
  {
    int jobId;
    char *submittime;
    long hours_remain;
    long mins_remain;
    long secs_remain;
    char *jobname;
    char *user;
    char *queue;
    int numProcessors;
    int ptile;
    int iflag :4;		/* iflag = 0: new job */
                                /* iflag = 1: idle    */ 
                                /* iflag = 2: held    */
                                /* iflag = 3: blocked */ 
  } koomie_job_struct;

  koomie_job_struct *Pend_jobs;

  if (named_user_jobs_only)
    strncpy(current_user,named_user.c_str(),255);
  else
    strncpy(current_user,getusername(),255);

  bool local_user_jobs_only=(user_jobs_only || named_user_jobs_only);
  
  if(local_user_jobs_only)
    printf("\nSUMMARY OF JOBS FOR USER: <%s>\n\n",current_user);
  
  //----------------
  // Query all jobs 
  //---------------

  job_info_msg_t *job_ptr = NULL;

  if( slurm_load_jobs( (time_t) NULL, &job_ptr, SHOW_ALL) )
    {
      printf("[Error]: unable to query SLURM jobs\n");
      exit(10);
    } 

  //slurm_print_job_info_msg(stdout,job_ptr,0);
  
  running_jobs = 1;

  std::string userString;

  /*--------------
   * showq Header 
   *--------------*/

  if(running_jobs)
    {
      printf("ACTIVE JOBS--------------------\n");
      if(long_listing)
	{
	  printf("JOBID     JOBNAME    USERNAME      STATE   CORES  NODES QUEUE          REMAINING STARTTIME\n");
	  printf("===================================================================================================\n");
	}
      else
	{
	  printf("JOBID     JOBNAME    USERNAME      STATE   NODES REMAINING STARTTIME\n");
	  printf("================================================================================\n");
	}

      /*-------------------------------
       * 1st: Display all running jobs 
       *-------------------------------*/
      
      for(int i=0; i<job_ptr->record_count;i++)
	{

	  job_info_t * job;
	  job = &job_ptr->job_array[i];

	  // get username string

	  userString = uid_to_string(job->user_id);

	// debugging missing jobs
        //  if (job->job_id == 6284384)
	//	printf("\nI catch you!!! reason=%d \n", job->state_reason);
 
	  // keep track of pending jobs...

	  if(job->job_state == JOB_PENDING)
	    {
	      num_pending_jobs++;
	      pending_jobs.push_back(i);
	      continue;
	    }

	  current_time = time(NULL);
	  secs_max     = job->time_limit*60; // max runtime (converted to secs)
	  remain_time  = (time_t)difftime(secs_max+job->start_time,current_time);

	  // keep track of completing jobs - those which have exceeded
	  // their runlimit by a given threshold are flagged as
	  // errored.

	  if(job->job_state & JOB_COMPLETING)
	    {
            //ignore non-user jobs with "-u" flag
            if ((local_user_jobs_only) &&
                   (strcmp(current_user,uid_to_string(job->user_id).c_str()) != 0) )
                    continue;
		
	      completing_jobs.push_back(i);

	      // allow 5 minute grace window

	      if(remain_time < 300) 
		continue;

	      errored_jobs.push_back(i);
	      continue;

	    }

	  // only displaying running jobs currently...

	  if( job->job_state != JOB_RUNNING )
	    continue;

	  if ((local_user_jobs_only) && 
	      (strcmp(current_user,uid_to_string(job->user_id).c_str()) != 0) )
	    continue;

	  // Gather time info
	  
	  starttime = strdup(ctime( &job->start_time));
	  starttime[strlen(starttime)-1] = '\0';
					  
	  hours_remain = remain_time/3600;
	  mins_remain  = (remain_time-hours_remain*3600)/60;
	  secs_remain  = (remain_time - hours_remain*3600 - mins_remain*60);
	  
	  if(hours_remain < 0 || mins_remain < 0 || secs_remain < 0)
	    {	 
	      printf("[Warn]: runlimit exhausted: runningpending job reason = %i\n",job->state_reason);
	      continue;
	    }

	  // Display job info

	  printf("%-10i", job->job_id);  

	  strncpy(jobname_short,job->name,10);
	  jobname_short[10] = '\0';
	  printf("%-11s",jobname_short); 

	  strncpy(jobuser_short,uid_to_string(job->user_id).c_str(),14);
	  jobuser_short[14] = '\0';
	  printf("%-14s", jobuser_short);
          
	  printf("%-8s","Running");
	  
	  if(long_listing)
		{
		printf("%-6i ",job->num_cpus);;
		}

          //print node number
          printf("%-5i",job->num_nodes);

          //print queue
	  if(long_listing)
	    {
	      //printf("%-4i",job->num_nodes);
	      //printf("%-4i",6000);
	      strncpy(queuename_short,job->partition,14);
	      queuename_short[14] = '\0';
	      printf(" %-14s",queuename_short); 
	    }

	  printf(" %2i:",hours_remain);
	  if(mins_remain < 10)
	    printf("0%1i:",mins_remain);
	  else
	    printf("%2i:",mins_remain);
	  
	  if(secs_remain < 10)
	    printf("0%1i  ",secs_remain);
	  else
	    printf("%2i  ",secs_remain);
	  
	  shorttime = (char *)calloc(20,sizeof(char));
	  strncpy(shorttime,starttime,19);
	  shorttime[19] = '\0';
	  printf("%s",shorttime);
	  
	  printf("\n");

	  free(starttime);
	  free(shorttime);
	  
	  rjobs++;
	  rprocs += job->num_cpus;
	  rnodes += job->num_nodes;

	} // end loop over slurm jobs

      /* Running Job summary*/
      
	if(local_user_jobs_only)
	  {
	    //printf("\nYou have %3i Active jobs utilizing: %6i of %6i Processors (%4.2f%%)\n",rjobs,rprocs,
	    //total_avail_procs,(float)100.*rprocs/total_avail_procs);
	  }
	else
	  {
	    printf("\n");
	    if(rjobs == 1)
	      printf("  %6i active job  ",rjobs);
	    else
	      printf("  %6i active jobs ",rjobs);

	    if(show_utilization)
	      printf(": %4i of %4i hosts (%6.2f %%)\n",rnodes,total_avail_nodes,(float)100.*rnodes/total_avail_nodes);
	    else
	      printf("\n");

	  }

    }

  //---------------------------------------------------------------------
  // 2nd: Display pending jobs; 1st come idle jobs waiting for free
  // resources, then come blocked jobs
  // ---------------------------------------------------------------------

  if(pending_jobs.size() > 0)
    { 
      printf("\nWAITING JOBS------------------------\n");
      if(long_listing)
	{
	  printf("JOBID     JOBNAME    USERNAME      STATE   CORES NODES QUEUE          WCLIMIT   QUEUETIME\n");
	  printf("==================================================================================================\n");
	}
      else
	{
	  printf("JOBID     JOBNAME    USERNAME      STATE   NODES WCLIMIT   QUEUETIME\n");
	  printf("================================================================================\n");
	}

      for(int i=0; i<pending_jobs.size();i++)
	{

	  job_info_t * job;
	  job = &job_ptr->job_array[pending_jobs[i]];

	  if(job->job_state != JOB_PENDING)
	    continue;

	  if ((local_user_jobs_only) && 
	      (strcmp(current_user,uid_to_string(job->user_id).c_str()) != 0) )
	    continue;

// Si Liu in Dec 2015:
// Update the new SLURM PENDING_REASONS since SLURM 15:
// For the following reasons, the job will not automatically run (This includes both blocked jobs and errored jobs!)
	  if(
	     job->state_reason == WAIT_DEPENDENCY      ||  /* 2, dependent job has not completed */
	     job->state_reason == WAIT_PART_NODE_LIMIT ||  /* 4  request exceeds partition node limit */
             job->state_reason == WAIT_PART_TIME_LIMIT ||  /* 5  request exceeds partition time limit */
	     job->state_reason == WAIT_HELD            ||  /* 8, job is held by administrator */
	     job->state_reason == WAIT_TIME            ||  /* 9,  job waiting for specific begin time */	   
             job->state_reason == WAIT_RESERVATION     ||  /* 14, reservation not available */
             job->state_reason == WAIT_NODE_NOT_AVAIL  ||  /* 15, required node is DOWN or DRAINED */
             job->state_reason == WAIT_HELD_USER       ||  /* 16, job is held by user */
	     job->state_reason == FAIL_DOWN_PARTITION  ||  /* 18, partition for job is DOWN */
	     job->state_reason == FAIL_DOWN_NODE       ||  /* 19, some node in the allocation failed */
	     job->state_reason == FAIL_BAD_CONSTRAINTS ||  /* 20, constraints can not be satisfied */
             job->state_reason == WAIT_DEP_INVALID	   /* 38, Dependency condition invalid or never*/
    	     )
	{
        //      if (job->job_id == 6284384)
	//	printf("\n\nI am still alive!\n\n");
	      blocked_jobs++;
	      continue;
	    }

// Si Liu in Dec 2015:
// For the following reasons, the job will finally run without extra manipulation (Jobs are reported as Waiting!)
	  if(
             job->state_reason != WAIT_NO_REASON       &&  /* 0,  not set or job not pending */
	     job->state_reason != WAIT_PRIORITY        &&  /* 1,  higher priority jobs exist */	
	     job->state_reason != WAIT_RESOURCES       &&  /* 3,  required resources not available */
	     job->state_reason != WAIT_PART_DOWN       &&  /* 6,  requested partition is down */
             job->state_reason != WAIT_PART_INACTIVE       /* 7,  requested partition is inactive */
	     )
	    {
	      printf("[Warn]: unknown job state - pending job reason = %i\n",job->state_reason);
	      continue;
	    }

	  idle_jobs++;

	  // Display job info

	  printf("%-10i", job->job_id);

	  strncpy(jobname_short,job->name,10);
	  jobname_short[10] = '\0';
	  printf("%-11s",jobname_short);

	  strncpy(jobuser_short,uid_to_string(job->user_id).c_str(),14);
	  jobuser_short[14] = '\0';
	  printf("%-14s", jobuser_short);

	  printf("%-8s","Waiting");

          if(long_listing)
		{
		printf("%-5i ",job->num_cpus);
		}	  
	  printf("%-5i",job->num_nodes);

	  if(long_listing)
	    {
//	      printf("%-4i ",job->num_nodes);
	      strncpy(queuename_short,job->partition,14);
	      queuename_short[14] = '\0';
	      printf(" %-14s",queuename_short); 
	    }

	  secs_max     = job->time_limit*60; // max runtime (converted to secs)
	  hours_remain = secs_max/3600;
	  mins_remain  = (secs_max-hours_remain*3600)/60;
	  secs_remain  = (secs_max - hours_remain*3600 - mins_remain*60);

	  printf(" %2i:",hours_remain);
	  if(mins_remain < 10)
	    printf("0%1i:",mins_remain);
	  else
	    printf("%2i:",mins_remain);
	  
	  if(secs_remain < 10)
	    printf("0%1i  ",secs_remain);
	  else
	    printf("%2i  ",secs_remain);

	  submittime = strdup(ctime( &job->submit_time));

	  shorttime = (char *)calloc(20,sizeof(char));
	  strncpy(shorttime,submittime,19);

	  printf("%s",shorttime);
	  printf("\n");

	  free(submittime);
	  free(shorttime);

	} //  end loop over slurm jobs

    }	  // end for pending jobs

  if(blocked_jobs > 0)
    {
      printf("\nBLOCKED JOBS--\n");
      if(long_listing)
	{
	  printf("JOBID     JOBNAME    USERNAME      STATE   CORES  NODES QUEUE          WCLIMIT   QUEUETIME\n");
	  printf("==================================================================================================\n");
	}
      else
	{
	  printf("JOBID     JOBNAME    USERNAME      STATE   NODES WCLIMIT   QUEUETIME\n");
	  printf("================================================================================\n");
	}

      for(int i=0; i<pending_jobs.size();i++)
	{

	  job_info_t * job;
	  job = &job_ptr->job_array[pending_jobs[i]];

	  if(job->job_state != JOB_PENDING)
	  	    continue;

//	if (job->job_id == 6284384)
//              printf("\n\nI am still alive!\n\n");

	  if ((local_user_jobs_only) &&
              (strcmp(current_user,uid_to_string(job->user_id).c_str()) != 0) )
            continue;
	
//Si Liu in Dec 2015: 
//We updated this in version 15 or later:
//The list of known errored jobs:  
         if(
	    job->state_reason == WAIT_PART_NODE_LIMIT ||  /* 4  request exceeds partition node limit */
            job->state_reason == WAIT_PART_TIME_LIMIT ||  /* 5  request exceeds partition time limit */
            job->state_reason == FAIL_BAD_CONSTRAINTS ||  /* 20, constraints can not be satisfied */
	    job->state_reason == WAIT_DEP_INVALID         /* 38, Dependency condition invalid or never*/
            )
	{
//	    if (job->job_id == 6284384)
//                printf("\n\nI am still alive!\n\n"); 
	    errored_jobs.push_back(pending_jobs[i]);
//	    printf("\n\ncurrent error job size %d new job id %d \n\n",errored_jobs.size(),job->job_id);
            continue;
	}

	if(
             job->state_reason != WAIT_DEPENDENCY &&     /* 2 dependent job has not completed */
	     job->state_reason != WAIT_HELD &&           /* 8 job is held by administrator */
	     job->state_reason != WAIT_TIME &&           /* 9 job waiting for specific begin time */
	     job->state_reason != WAIT_RESERVATION &&    /* 14 reservation not available */
	     job->state_reason != WAIT_NODE_NOT_AVAIL && /* 15 required node is DOWN or DRAINED */
	     job->state_reason != WAIT_HELD_USER &&      /* 16 job is held by user */
	     job->state_reason != FAIL_DOWN_PARTITION && /* 18 partition for job is DOWN */ 
	     job->state_reason != FAIL_DOWN_NODE         /* 19 some node in the allocation failed */
    	   )
	    {
	      continue;
	    }

	  // Display job info

	  printf("%-10i", job->job_id);

	  strncpy(jobname_short,job->name,10);
	  jobname_short[10] = '\0';
	  printf("%-11s",jobname_short);

	  strncpy(jobuser_short,uid_to_string(job->user_id).c_str(),14);
	  jobuser_short[14] = '\0';
	  printf("%-14s", jobuser_short);

//	  printf("%-8s","Blocked");

	  switch( job->state_reason )
	  {
	  case WAIT_DEPENDENCY:
		printf("%-8s","WaitDep");
		break;
	  case WAIT_HELD:
		printf("%-8s","HeldAdm");
                break;
	  case WAIT_TIME:
		printf("%-8s","WaitTim");
                break;
	  case WAIT_RESERVATION:	  
		printf("%-8s","WaitRes");
                break;
	  case WAIT_NODE_NOT_AVAIL:
		printf("%-8s","WaitNod");
                break;
	  case WAIT_HELD_USER:
		printf("%-8s","HeldUsr");
                break;
	  case FAIL_DOWN_PARTITION:
		printf("%-8s","DownPar");
                break;
	  case FAIL_DOWN_NODE:
		printf("%-8s","DownNode");
		break;
	  default:
		printf("%-8s","Blocked");
		break;
	  }
	 
          if(long_listing)
		{
		printf("%-6i ",job->num_cpus);
		}
          
          printf("%-5i",job->num_nodes);

	  if(long_listing)
	    {
	      strncpy(queuename_short,job->partition,14);
	      queuename_short[14] = '\0';
	      printf(" %-14s",queuename_short); 
	    }

	  secs_max     = job->time_limit*60; // max runtime (converted to secs)
	  hours_remain = secs_max/3600;
	  mins_remain  = (secs_max-hours_remain*3600)/60;
	  secs_remain  = (secs_max - hours_remain*3600 - mins_remain*60);

	  printf(" %2i:",hours_remain);
	  if(mins_remain < 10)
	    printf("0%1i:",mins_remain);
	  else
	    printf("%2i:",mins_remain);
	  
	  if(secs_remain < 10)
	    printf("0%1i  ",secs_remain);
	  else
	    printf("%2i  ",secs_remain);

	  submittime = strdup(ctime( &job->submit_time));

	  shorttime = (char *)calloc(20,sizeof(char));
	  strncpy(shorttime,submittime,19);

	  printf("%s",shorttime);
	  printf("\n");

	  free(submittime);
	  free(shorttime);
	} // end loop over jobs

    }

  //-----------------------------------------------------
  // 4th: Summarize completing/errored jobs
  //----------------------------------------------------

  if(errored_jobs.size() > 0)
    {
      //printf("errored job size %d\n", errored_jobs.size());
      printf("\nCOMPLETING/ERRORED JOBS-------------\n");
      printf("JOBID     JOBNAME    USERNAME      STATE   NODES   WCLIMIT  QUEUETIME\n");
      printf("================================================================================\n");

      for(int i=0; i<errored_jobs.size();i++)
	{
	  job_info_t * job;
	  job = &job_ptr->job_array[errored_jobs[i]];

//	  printf("\n\n stored error job size %d stored job id %d \n\n",errored_jobs.size(),job->job_id);

	  current_time = time(NULL);
	  secs_max     = job->time_limit*60; // max runtime (converted to secs)
	  remain_time  = (time_t)difftime(secs_max+job->start_time,current_time);
	  starttime    = strdup(ctime( &job->start_time));

	  starttime[strlen(starttime)-1] = '\0';
					  
	  hours_remain = remain_time/3600;
	  mins_remain  = (remain_time-hours_remain*3600)/60;
	  secs_remain  = (remain_time - hours_remain*3600 - mins_remain*60);

	  /* only show leading minus sign */
	 
         // debug:
	 //if (job->job_id == 6284384)
         //       printf("\n\nI am still alive!\n\n");

//         if (job->job_id == 6302330)
//	        {printf ("start_time: %d, secs_max : %d \n", job->start_time, secs_max);
//		printf ("current time: %d , remain time: %d \n ", current_time, remain_time);
//		}
	  if(mins_remain < 0)
	    mins_remain = -mins_remain;
	  if(secs_remain < 0)
	    secs_remain = -secs_remain;

	  strncpy(jobname_short,job->name,10);
	  jobname_short[10] = '\0';
	  strncpy(jobuser_short,uid_to_string(job->user_id).c_str(),14);
	  jobuser_short[14] = '\0';

	  printf("%-10i", job->job_id);
	  printf("%-11s",jobname_short);
	  printf("%-14s", jobuser_short);
//	  printf("%-8s","Complte");
	   
	  switch(job->state_reason)
	  {
	  case WAIT_PART_NODE_LIMIT:  /* 4  request exceeds partition node limit */
                printf("%-8s","ErrNode");
                break;
	  case WAIT_PART_TIME_LIMIT:  /* 5  request exceeds partition time limit */
		printf("%-8s","ErrTime");
                break;
	  case FAIL_BAD_CONSTRAINTS: /* 20, constraints can not be satisfied */
		printf("%-8s","ErrCons");
                break;
          case WAIT_DEP_INVALID:     /* 38, Dependency condition invalid or never*/
		printf("%-8s","ErrDep");
                break;
	  default:
		printf("%-8s","Complte");
		break;
	  } 

          printf("%-5i ",job->num_nodes);

	  if (hours_remain>0)
	  {
	  printf(" %2i:",hours_remain);
	  if(mins_remain < 10)
	    printf("0%1i:",mins_remain);
	  else
	    printf("%2i:",mins_remain);
	  
	  if(secs_remain < 10)
	    printf("0%1i  ",secs_remain);
	  else
	    printf("%2i  ",secs_remain);
	  }
	  else
	  { 
	 	printf("  Unknown  ");
	  }
	  shorttime = (char *)calloc(20,sizeof(char));
	  strncpy(shorttime,starttime,19);
	  shorttime[19] = '\0';
	  printf("%s",shorttime);
	  printf("\n");

	  free(starttime);
	  free(shorttime);

	} // end loop over errored jobs
    
    }

  /*----------
   * Summary
   *----------*/
  
  printf("\nTotal Jobs: %-4i  Active Jobs: %-4i  Idle Jobs: %-4i  "
	 "Blocked Jobs: %-4i\n",rjobs+idle_jobs+blocked_jobs,
	 rjobs,idle_jobs,blocked_jobs);
//
//  /*------------------------------
//   * Query advanced reservations 
//   *------------------------------*/
//
//    resInfo = lsb_reservationinfo(NULL, &resvNumbers, 0);
//
//    if(resvNumbers > 0)
//      {
//
//	printf("\nADVANCED RESERVATIONS----------\n");
//	printf("RESV ID     PROC                    RESERVATION WINDOW\n");
//	
//	for(i=0;i<resvNumbers;i++)
//	  {
//	    sscanf(resInfo[i].timeWindow,"%ld-%ld",&time1,&time2);
//
//	    /* Weed out reservations that are a long way out */
//
//	    if( (difftime(time1,time(NULL))) > RES_LEAD_WINDOW && !show_all_reserv)
//	      continue;
//
//	    starttime = strdup(ctime(&time1));
//	    endtime   = strdup(ctime(&time2));
//	    starttime[strlen(starttime)-1] = '\0';
//	    endtime[strlen(endtime)-1]     = '\0';
//
//	    /* Count up the number of reserved processors */ 
//
//	    reserv_procs = 0;
//	    for(j=0;j<resInfo[i].numRsvHosts;j++)
//	      reserv_procs += resInfo[i].rsvHosts[j].numRsvProcs;
//
//	    printf("%-10s  %4i    %s - %s\n",resInfo[i].rsvId,
//		   reserv_procs,starttime,endtime);
//
//
//
//	    free(starttime);
//	    free(endtime);
//	      
//	  }
//      }

  // clean-up time

  slurm_free_job_info_msg(job_ptr);


//More work to do
//  exit(0);

}

/*-------------------------------------------------------------------
 * quantify_pending_job(): 
 * 
 * Determine job idleness by comparing lsb_pendreason to certain 
 * key pending reasons.  Idle jobs are defined as:
 *
 *    (a) those that do not have enough processes available
 *    (b) freshly submitted jobs
 *
 * Deferred jobs are the inverse.
 *---------------------------------------------------------------------*/

int quantify_pending_job(char *reason)
{

  const char *idle_string1 = "New job is waiting for scheduling";
  const char *idle_string2 = "Not enough processors";
  const char *idle_string3 = "Job was suspended";
  const char *idle_string4 = "Job\'s requirement for exclusive execution not satisfied";
  char *local;

  if(strstr(reason,idle_string1) != NULL)
    return(0);
  else if(strstr(reason,idle_string2) != NULL)
    return(1);
  else if(strstr(reason,idle_string3) != NULL)
    return(2);
  else if(strstr(reason,idle_string4) != NULL)
    return(1);
  else
    return(3);
}

/*-------------------------------------
 * Parse/Define command line arguments
 *-------------------------------------*/

#if 0
void parse_command_line(int argc,char **argv)
{
  /* Allowable command line options */

#if 1
  struct arg_lit *l        = arg_lit0("l", NULL, "long (wide) listing");
  struct arg_lit *u        = arg_lit0("u", NULL, "display jobs for "
				                 "current user only");
  struct arg_lit *all_res  = arg_lit0(NULL,"all-reserv",
				           "show all advanced reservations"); 
  struct arg_lit *help     = arg_lit0(NULL,"help","print this help and exit");
  struct arg_lit *version  = arg_lit0(NULL,"version",
				      "print version information and exit");
  struct arg_end *end      = arg_end(20);
  void* argtable[]         = {l,u,all_res,help,version,end};
  const char* progname     = "slurm_showq";
  int exitcode=0;
  int nerrors;
#endif

  /* verify the argtable[] entries were allocated sucessfully */

  if (arg_nullcheck(argtable) != 0)
    {
      printf("%s: insufficient memory\n",progname);
      exit(1);
    }

  /* set any command line default values prior to parsing */

  /* Parse the command line as defined by argtable[] */

  nerrors = arg_parse(argc,argv,argtable);

  /* special case: '--help' takes precedence over error reporting */

  if (help->count > 0)
    {
      printf("\nUsage: %s ", progname);
      arg_print_syntax(stdout,argtable,"\n");
      printf("\nshowq summarizes all running, idle, and pending jobs \n");
      printf("along with any advanced reservations "
	     "in the SLURM batch system.\n\n");
      arg_print_glossary(stdout,argtable,"  %-25s %s\n");
      printf("\n");
      exit(0);
    }

  /* special case: '--version' takes precedence error reporting */

  if (version->count > 0)
    {
      printf("\n%s: Version %2.1f\n",progname,VERSION);
      printf("Texas Advanced Computing Center, "
	     "The University of Texas at Austin\n\n");
      exit(0);
    }

  /* If the parser returned any errors then display them and exit */

  if (nerrors > 0)
    {
      arg_print_errors(stdout,end,progname);
      printf("Try '%s --help' for more information.\n",progname);
      exit(1);
    }

  /* normal case: take the command line options at face value */

  if(l->count > 0)
    long_listing = 1;
  
  if(u->count > 0)
    user_jobs_only = 1;

  if(all_res->count > 0)
    show_all_reserv = 1;

  return;

}
#endif

char *Slurm_Showq::getusername() 
{
  char *user = 0;
  struct passwd *pw;

  pw = getpwuid(getuid());

  if (pw)
    user = pw->pw_name;
  return user;
}

std::string Slurm_Showq::uid_to_string(uid_t id)
{
  std::string username="nobody";
  struct passwd *pwd;

  if( (pwd = getpwuid(id)) == NULL)
    printf("Error: unable to ascertain user for uid = %u\n",id);
  else
    {
      username = pwd->pw_name;
    }

  return(username);
      
}

//-------------------------------------------------------------
// parse_supported_options: Parse/Define command line arguments
//-------------------------------------------------------------

void Slurm_Showq::parse_supported_options(int argc, char *argv[])
{

  GetPot cl(argc,argv);

  if(cl.search("--version"))
    print_version();

  if(cl.search(2,"--long","-l"))
    long_listing = true;

  if(cl.search(2,"--help","-h"))
    print_usage();

  if(cl.search(2,"-u","--user"))
    user_jobs_only = true;

  if (cl.search("-U"))
    {
      named_user_jobs_only=true;
      named_user=cl.next("");
    }

  if (user_jobs_only && named_user_jobs_only)
    {
      fprintf(stderr,"\n -U and -u are mutually exclusive options. Exiting.\n");
      print_usage();
    }
  

}

//-------------------------------------------------------------
// print_usage: show command options
//-------------------------------------------------------------

void Slurm_Showq::print_usage()
{
  printf("\nUsage: %s [OPTIONS]\n\n", progname.c_str());
  printf("Thus utility summarizes all running, idle, and pending jobs \n");
  printf("along with any upcoming advanced reservations in the SLURM batch system.\n\n");
	 
  printf("OPTIONS:\n");
  printf("  --help                  generate help message and exit\n");
  printf("  --version               output version information and exit\n");
  printf("  --all-reserv            show all advanced reservations\n");
  printf("  -l [ --long ]           enable more verbose (long) listing\n");
  printf("  -u [ --user ]           display jobs for current user only\n");
  printf("  -U <username>           display jobs for username only\n");
  
  printf("\n");
  exit(0);
}

//-------------------------------------------------------------
// print_version: echo current version
//-------------------------------------------------------------

void Slurm_Showq::print_version()
{
  printf("\n--------------------------------------------------\n");
  printf("%s: Version %2.1f\n\n",progname.c_str(),VERSION);
  printf("Texas Advanced Computing Center\n");
  printf("The University of Texas at Austin\n");
  printf("--------------------------------------------------\n");
  printf("\n");

  exit(0);

  return;
}

//-----------------------------------------------------------------
// read_runtime_config()
//----------------------------------------------------------------- 

void Slurm_Showq::read_runtime_config(std::string config_file)
{

  // determine where binary is being run from

  string exe = get_executable_path();

  char *exe_tmp = new char[exe.size() + 1];
  std::copy(exe.begin(),exe.end(),exe_tmp);

  exe_tmp[exe.size()] = '\0';

  std::string ifile(dirname(exe_tmp));

  delete [] exe_tmp;
  
  ifile += "/" + config_file;

  FILE *fp = fopen(ifile.c_str(),"r");

  if(fp == NULL)
    return;
  else
      {
	GetPot parse(ifile.c_str(),"#","\n");
	//parse.print();

	if(parse.size() <= 1)   // empty config file
	  return;		

	int num_hosts = parse("hosts_available",0);

	if(num_hosts > 0)
	  {
	    total_avail_nodes = num_hosts;
	    show_utilization = true;
	  }
      }

  return;
}

std::string Slurm_Showq::get_executable_path()
{
  char result[ PATH_MAX ];
  ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX );
  return std::string( result, (count > 0) ? count : 0 );
}

//-----------------------------------------------------------------
// Slurm_Showq(): Default constructor
//----------------------------------------------------------------- 

Slurm_Showq::Slurm_Showq()
{
  // default values

  long_listing     = false;
  user_jobs_only   = false;
  named_user_jobs_only = false;
  show_all_reserv  = false;
  show_utilization = false;

  RES_LEAD_WINDOW = 24*7*3600;	// 7-day lead time

  progname           = "slurm_showq";

  // read additional runtime settings from config file if present

  read_runtime_config("showq.conf");

  return;
}

void Slurm_Showq::check_maintenance()
{
  string maintenance_info;  
  if (system("scontrol show reservation | grep -i maintenance > /dev/null")==0)
    	{
        printf("\n\n-------------------- WARNING ! --------------------\n\n");
    	printf("The following system maintenance has been scheduled:\n");
        maintenance_info=GetStdoutFromCommand("scontrol show reservation | grep -i maintenance");
	printf("%s", maintenance_info.c_str());
        printf("\nYour jobs may only be scheduled to run after the system maintenance,\n");
	printf("  if these jobs can not be run before the maintenance.\n\n"); 	
	}
}




string Slurm_Showq::GetStdoutFromCommand(string cmd) 
{
  string data;
  FILE * stream;
  const int max_buffer = 256;
  char buffer[max_buffer];
  cmd.append(" 2>&1");

  stream = popen(cmd.c_str(), "r");
  if (stream) 
    {
    while (!feof(stream))
      if  (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
        pclose(stream);
    }
  return data;
}

