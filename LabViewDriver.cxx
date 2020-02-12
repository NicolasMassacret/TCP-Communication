/********************************************************************\
  Name:         LabViewDriver.cxx
  Created by:   Nicolas Massacret
  $Id$
                Handle communication with LabVIEW through TCP, to monitor LabVIEW variables or send TPC request.
                
\********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <string>
#include "midas.h"
#include "mfe.h"
#include <stdexcept>

#include "LabViewDriver.h"

#include "tmfe.h"
#include "KOtcp.h"
#include "feTCP.h"

#define NCH 12
uint16_t nch =  NCH;

class feLabview : public feTCP
{
public:
  feLabview(TMFE* mfe, TMFeEquipment* eq):feTCP(mfe, eq){} // ctor

  void Init()
  {
    fEventSize = 100;
    if (fEventBuf) {
      free(fEventBuf);
    }
    fEventBuf = (char*)malloc(fEventSize);
  }

  void HandleBeginRun()
  {
    fMfe->Msg(MINFO, "HandleBeginRun", "Begin run!");
    fEq->SetStatus("Running", "#00FF00");
  }

  void HandleEndRun()
  {
    fMfe->Msg(MINFO, "HandleEndRun", "End run!");
    fEq->SetStatus("Stopped", "#00FF00");
  }

  void HandlePeriodic()
  {
    printf("periodic!\n");
  }

  string HandleRpc(const char* cmd, const char* args)
  {
    fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
    string response = "OK";
    return response;
  }
private:
   
  // string tolower(string data){
      
  //    return data;
  // }
};

/*-- Globals -------------------------------------------------------*/

/* The frontend name (client name) as seen by other MIDAS clients   */
#define FENAME "Motor_Cryodoor"	// Equipment list cannot have variable inside, so need #define constant
char const *frontend_name = FENAME;
/* The frontend file name, don't change it */
char const *frontend_file_name = __FILE__;

/* frontend_loop is called periodically if this variable is TRUE    */
BOOL frontend_call_loop = TRUE;

/* a frontend status page is displayed with this frequency in ms */
INT display_period = 0000;

/* maximum event size produced by this frontend */
INT max_event_size = 10000;

/* maximum event size for fragmented events (EQ_FRAGMENTED) */
INT max_event_size_frag = 5 * 1024 * 1024;

/* buffer size to hold events */
INT event_buffer_size = 100 * 10000;

HNDLE hDD, hSet, hVar, hRotSet;
  
LABVIEWDRIVER_SETTINGS LabDriver_settings;
LABVIEWDRIVER_VARIABLES LabDriver_variables;
  
feLabview* myfe;
char monitored_var_str[80];
char destination_var_str[80];
char dynamic_set_str[80];
char rot_set_str[80];
  
/*-- Function declarations -----------------------------------------*/
INT frontend_init();
INT frontend_exit();
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop();
  
INT read_LabDriver_event(char *pevent, INT off);
INT local_LabDriver_init(char const *eqname);
void param_callback(INT hDB, INT hKey, void *info);
INT handleModule(INT nch);
  
  
  
/*-- Equipment list ------------------------------------------------*/

EQUIPMENT equipment[] = {
  {FENAME,                 /* equipment name */
   {4, 0,                   /* event ID, trigger mask */
    "SYSTEM",                      /* event buffer */
    EQ_PERIODIC,   /* equipment type */
    0,                      /* event source */
    "MIDAS",                /* format */
    TRUE,                   /* enabled */
    RO_ALWAYS |   /* read when running and on transitions */
    RO_ODB,                 /* and update ODB */
    1000,                  /* read every  sec */
    0,                      /* stop run after this event limit */
    0,                      /* number of sub events */
    1,                      /* log history */
    "", "", "",},
   read_LabDriver_event,       /* readout routine */
  },
    
  {""}
};

/********************************************************************\
              Callback routines for system transitions

  These routines are called whenever a system transition like start/
  stop of a run occurs. The routines are called on the following
  occations:

  frontend_init:  When the frontend program is started. This routine
                  should initialize the hardware.

  frontend_exit:  When the frontend program is shut down. Can be used
                  to releas any locked resources like memory, commu-
                  nications ports etc.

  begin_of_run:   When a new run is started. Clear scalers, open
                  rungates, etc.

  end_of_run:     Called on a request to stop a run. Can send
                  end-of-run event and close run gates.

  pause_run:      When a run is paused. Should disable trigger events.

  resume_run:     When a run is resumed. Should enable trigger events.
\********************************************************************/

/*-- Sequencer callback info  --------------------------------------*/
void seq_rotation_callback(INT hDB, INT hseq, void *info)
{
  printf("odb ... Rotation motor parameter touched \n");
  printf("Position requested: %s \n", LabDriver_settings.Rot_position);
  cm_msg(MINFO, "seq_rotation_callback", "Position requested: %s \n", LabDriver_settings.Rot_position);
        
  string resp=myfe->Exchange(string("rotation_")+LabDriver_settings.Rot_position+"\r\n");
        
  //char respond;
  std::string respond_str(resp.c_str());
            
  printf("Value returned: %s \n", respond_str.c_str());
}

/*-- Frontend Init -------------------------------------------------*/
INT frontend_init()
{
  int status;
  // Deal with TCP connection
  
  printf("initialize TCP connection \n");
  TMFE* mfe = TMFE::Instance();
  TMFeError err = mfe->Connect(frontend_name, __FILE__);
  if (err.error) {
    printf("Cannot connect, bye.\n");
    return 1;
  }

  //mfe->SetWatchdogSec(0);

  TMFeCommon *common = new TMFeCommon();
  common->EventID = 1;
  common->LogHistory = 1;
  //common->Buffer = "SYSTEM";
   
  TMFeEquipment* eq = new TMFeEquipment(mfe, frontend_name, common);
  eq->Init();
  eq->SetStatus("Starting...", "white");
  eq->ZeroStatistics();
  eq->WriteStatistics();
   
  mfe->RegisterEquipment(eq);
    
  //feLabview* 
  myfe = new feLabview(mfe, eq);
  mfe->RegisterRpcHandler(myfe);
  myfe->TCPConnect();
  //string resp=myfe->Exchange(string("read m1")+"\r\n");
  status = local_LabDriver_init(frontend_name);
   
   
   
   

  return status;
}

/*-- Frontend Exit -------------------------------------------------*/
INT frontend_exit()
{
  
  return SUCCESS;
}

/*-- Begin of Run --------------------------------------------------*/
INT begin_of_run(INT run_number, char *error)
{
  /* put here clear scalers etc. */
  return SUCCESS;
}

/*-- End of Run ----------------------------------------------------*/
INT end_of_run(INT run_number, char *error)
{
  return SUCCESS;
}

/*-- Pause Run -----------------------------------------------------*/
INT pause_run(INT run_number, char *error)
{
  return SUCCESS;
}

/*-- Resume Run ----------------------------------------------------*/
INT resume_run(INT run_number, char *error)
{
  return SUCCESS;
}

/*-- Frontend Loop -------------------------------------------------*/
INT frontend_loop()
{
  /* if frontend_call_loop is true, this routine gets called when
     the frontend is idle or once between every event */
  ss_sleep(100);
  return SUCCESS;
}

/*------------------------------------------------------------------*/
// Readout routines for different events
/*-- Trigger event routines ----------------------------------------*/

INT poll_event(INT source, INT count, BOOL test)
/* Polling routine for events. Returns TRUE if event
   is available. If test equals TRUE, don't return. The test
   flag is used to time the polling */
{
  int i;
  DWORD lam;
  
  for (i = 0; i < count; i++) {
    lam = 1;
    if (lam & LAM_SOURCE_STATION(source))
      if (!test)
	return lam;
  }
  return 0;
}

/*-- Interrupt configuration ---------------------------------------*/
INT interrupt_configure(INT cmd, INT source, POINTER_T adr)
{
  switch (cmd) {
  case CMD_INTERRUPT_ENABLE:
    break;
  case CMD_INTERRUPT_DISABLE:
    break;
  case CMD_INTERRUPT_ATTACH:
    break;
  case CMD_INTERRUPT_DETACH:
    break;
  }
  return SUCCESS;
}

/*-- MSCB event --------------------------------------------------*/
INT read_LabDriver_event(char *pevent, INT off)
{
  /* init bank structure */
  


  int  size;
  float value;
    
  if(LabDriver_settings.dynamic==true)
    {
      /*Update variables to monitor and destination in ODB*/
      sprintf(monitored_var_str, "/Equipment/%s/Settings/LabV_monitored_variables", frontend_name);
      db_find_key(hDB, 0, monitored_var_str, &hVar);
      size=sizeof(LabDriver_settings.LabVar);
      db_get_record(hDB, hVar, &LabDriver_settings.LabVar, &size, 0);
    }
        
  /*Read variables and write their value in ODB*/
  for(int a=0; a<12; a=a+1)
    {
      if(string(LabDriver_settings.LabVar[a])!="" )
        {
	  //printf("Reading variable : %s \n", LabDriver_settings.LabVar[a]);
	  string resp=myfe->Exchange(string("read_")+LabDriver_settings.LabVar[a]+"\r\n");
	  std::string value_str(resp.c_str());
	  try{
	    value = std::stof(value_str);
	  }
	  catch (const std::invalid_argument& ia){
	    value=-99999;
	    printf("Received empty message from client. \n");
	  }
            
	  printf("Value returned: %f \n", value);
            
	  sprintf(destination_var_str, "/Equipment/%s/Variables/LabV_var_val", frontend_name);
	  db_find_key(hDB, 0, destination_var_str, &hVar);
	  size=sizeof(LabDriver_variables.LabVarValue);
	  db_get_record(hDB, hVar, &LabDriver_variables.LabVarValue, &size, 0);
	  LabDriver_variables.LabVarValue[a]=value;
	  db_set_value(hDB, 0, destination_var_str, LabDriver_variables.LabVarValue, sizeof(LabDriver_variables.LabVarValue), nch, TID_FLOAT);
        }
    }
    
  // /* Write data bank*/
  bk_init(pevent);
  float *pfdata;
  bk_create(pevent, "VLAB", TID_FLOAT, (void **)&pfdata);
  for (int i=0; i<nch; i++) {
    *pfdata++ = (float) (LabDriver_variables.LabVarValue[i]);
  }
  bk_close(pevent, pfdata);

  return bk_size(pevent);
}

/*-- Local MSCB event --------------------------------------------------*/
INT local_LabDriver_init(char const *eqname)
{
  int  status, size;
  char set_str[80], var_str[80];
  LABVIEWDRIVER_SETTINGS_STR(LabDriver_settings_str);
  LABVIEWDRIVER_VARIABLES_STR(LabDriver_variables_str);
  
  cm_get_experiment_database(&hDB, NULL);
  
  sprintf(set_str, "/Equipment/%s/Settings", eqname);
  status = db_create_record(hDB, 0, set_str, strcomb(LabDriver_settings_str));
  status = db_find_key(hDB, 0, set_str, &hSet);
  
  printf("Getting the dynamic variable value. \n");
  sprintf(dynamic_set_str, "/Equipment/%s/Settings/Dynamic_mode", eqname);
  db_find_key(hDB, 0, dynamic_set_str, &hSet);
  size=sizeof(LabDriver_settings.dynamic);
  db_get_record(hDB, hSet, &LabDriver_settings.dynamic, &size, 0);
  printf("Getting the dynamic variable value. Bis \n");
  
  //------
  sprintf(rot_set_str, "/Equipment/%s/Settings/Rotation_position", eqname);
  db_find_key(hDB, 0, rot_set_str, &hRotSet);
  size = sizeof(LabDriver_settings.Rot_position);
  db_open_record(hDB, hRotSet, &LabDriver_settings.Rot_position, size, MODE_READ, seq_rotation_callback, NULL);
  seq_rotation_callback(hDB, hRotSet, NULL);
  
  
  // sprintf(var_str, "/Equipment/%s/Variables", eqname);
  // status = db_create_record(hDB, 0, var_str, strcomb(LabDriver_variables_str));
  // status = db_find_key(hDB, 0, var_str, &hVar);

  sprintf(var_str, "/Equipment/%s/Variables", eqname);
  status = db_create_record(hDB, 0, var_str, strcomb(LabDriver_variables_str));
  status = db_find_key(hDB, 0, var_str, &hVar);
   
  size=sizeof(LabDriver_variables);
  db_get_record(hDB, hVar, &LabDriver_variables, &size, 0);

  //sprintf(var_str, "%s", varname);
  //printf("%s", var_str);
  /*-- Enable Hotlink ------------------------------------------------------------------*/
   
  //
  //size = sizeof(LabDriver_settings.position);
  //db_open_record(hDB, hRotSet, &LabDriver_settings.position, size, MODE_READ, seq_rotation_callback, NULL);
  //db_open_record(hDB, hImax, &(v6533_settings.imax), size, MODE_READ, seq_imaxcallback, NULL);


   
  /*----------------*/
   
  //test 
   
    

    
  /*sprintf(destination_var_str, "/Equipment/LabViewDriver/Settings/Destination_in_ODB");
    db_find_key(hDB, 0, destination_var_str, &hVar);
    size=sizeof(LabDriver_settings.OdbDest);
    db_get_record(hDB, hVar, &LabDriver_settings.OdbDest, &size, 0);
  */
  return status;  
}
