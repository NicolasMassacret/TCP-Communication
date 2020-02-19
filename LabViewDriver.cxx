//
// LabViewDriver
//
// Frontend for interfacing with LabView over TCP
//

#include <stdio.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()
#include <stdlib.h> // malloc()
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

#include "midas.h"
#include "tmfe.h"

#include "KOtcp.h"
#include "feTCP.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

#define NCH 12

class feLabview :
   public feTCP
{
public:
   TMFE* fMfe;
   TMFeEquipment* fEq;

   int fEventSize;
   char* fEventBuf;

   feLabview(TMFE* mfe, TMFeEquipment* eq):feTCP(mfe, eq) // ctor
   {
      fMfe = mfe;
      fEq  = eq;
      fEventSize = 0;
      fEventBuf  = NULL;
   }

   ~feLabview() // dtor
   {
      if (fEventBuf) {
         free(fEventBuf);
         fEventBuf = NULL;
      }
   }

   void Init()
   {
      fEventSize = 100;
      fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
      if (fEventBuf) {
         free(fEventBuf);
      }
      fEventBuf = (char*)malloc(fEventSize);

      settings.LabVar.resize(NCH);
      settings.names.resize(NCH);
      variables.resize(NCH);
      fEq->fOdbEqSettings->RSA("LabV_monitored_variables", &settings.LabVar, true, NCH);
      fEq->fOdbEqSettings->RB("Dynamic_mode", &settings.dynamic, true);
      fEq->fOdbEqSettings->RF("Rotation_position", &settings.Rot_position, true);
      fEq->fOdbEqSettings->RF("Translation_position", &settings.Trans_position, true);
      fEq->fOdbEqVariables->RDA("LabV_var_val", &variables, true, NCH);

   }

   void SendEvent(double dvalue)
   {
      fEq->ComposeEvent(fEventBuf, fEventSize);
      fEq->BkInit(fEventBuf, fEventSize);

      double* ptr = (double*)fEq->BkOpen(fEventBuf, "test", TID_DOUBLE);
      *ptr = dvalue;
      fEq->BkClose(fEventBuf, ptr+1);

      fEq->SendEvent(fEventBuf);
   }

   std::string HandleRpc(const char* cmd, const char* args)
   {
      fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
      return "OK";
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
      // printf("periodic!\n");
      read_event();
      //char buf[256];
      //sprintf(buf, "buffered %d (max %d), dropped %d, unknown %d, max flushed %d", gUdpPacketBufSize, fMaxBuffered, fCountDroppedPackets, fCountUnknownPackets, fMaxFlushed);
      //fEq->SetStatus(buf, "#00FF00");
      //fEq->WriteStatistics();
   }

   void fecallback(HNDLE hDB, HNDLE hkey, INT index);
   INT read_event();

   bool Handshake(){
      cout << "HANDSHAKE" << endl;
      string resp = Exchange("MIDAS\r\n");
      bool correct = (resp.substr(0,7) == string("LabView"));
      if(!correct) fMfe->Msg(MERROR, "Handshake", "Unexpected response: %s", resp.c_str());
      return correct;
   }
private:
   // INT read_event(char *pevent, INT off);
   typedef struct {
      string      hostname;
      string    port;
      bool      dynamic;

      float    Rot_position;
      float     Trans_position;
      vector<string>     LabVar;
      vector<string>     names;
   } ldsettings;
   ldsettings settings;

   vector<double> variables;
};

void feLabview::fecallback(HNDLE hDB, HNDLE hkey, INT index)
{
   KEY key;
   int status = db_get_key(hDB, hkey, &key);
   char reqstr[256];
   if(status == DB_SUCCESS){
      switch(key.type){
      case TID_BOOL:
         {
            bool val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            sprintf(reqstr, (val?"true":"false"));
            break;
         }
      case TID_INT:
         {
            int val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            sprintf(reqstr, "%d", val);
            break;
         }
      case TID_DOUBLE:
         {
            double val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            sprintf(reqstr, "%f", val);
            settings.Rot_position = val;
            break;
         }
      case TID_FLOAT:
         {
            float val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            sprintf(reqstr, "%f", val);
            settings.Rot_position = val;
            break;
         }
      case TID_STRING:
         {
            int size = 256;
            db_get_data(hDB, hkey, (void*)&reqstr, &size, key.type);
            break;
         }
      case TID_WORD:
         {
            uint16_t val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            sprintf(reqstr, "%d", val);
            break;
         }
      case TID_DWORD:
         {
            uint32_t val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            sprintf(reqstr, "%d", val);
            break;
         }
      }
      cm_msg(MINFO, "callback", "Change requested: %s [ %d ] -> %s\n", key.name, index, reqstr);

      std::ostringstream oss;
      oss << "rotation_" << settings.Rot_position << "\r\n";
      string resp=Exchange(oss.str());
   }
   // //char respond;
   // std::string respond_str(resp.c_str());

   // printf("Value returned: %s \n", respond_str.c_str());
}

// INT feLabview::read_event(char *pevent, INT off)
INT feLabview::read_event()
{
   double value;

   if(settings.dynamic==true)
      {
         /*Update variables to monitor and destination in ODB*/
         fEq->fOdbEqSettings->RSA("LabV_monitored_variables", &settings.LabVar, true, NCH);
      }

   /*Read variables and write their value in ODB*/
   for(unsigned int a=0; a<settings.LabVar.size(); a++)
      {
         if(settings.LabVar[a].size())
            {
               // printf("Reading variable : %s \n", settings.LabVar[a].c_str());
               string resp=Exchange(string("read_")+settings.LabVar[a]+"\r\n");
               try{
                  value = std::stod(resp);
               }
               catch (const std::invalid_argument& ia){
                  value=-99999;
                  printf("Received empty message from client. \n");
               }

               printf("Value returned: %f \n", value);

               variables[a]=value;
            }
      }
   fEq->fOdbEqVariables->WDA("LabV_var_val", variables);

   return 0;
}

static void usage()
{
   fprintf(stderr, "Usage: LabViewDriver_tmfe.exe <name> ...\n");
   exit(1);
}

void callback(INT hDB, INT hkey, INT index, void *feptr)
{
   cout << "CALLBACK" << endl;
   feLabview* fe = (feLabview*)feptr;
   fe->fecallback(hDB, hkey, index);
}

int main(int argc, char* argv[])
{
   setbuf(stdout, NULL);
   setbuf(stderr, NULL);

   signal(SIGPIPE, SIG_IGN);

   std::string name = "";

   if (argc == 2) {
      name = argv[1];
   } else {
      usage(); // DOES NOT RETURN
   }

   TMFE* mfe = TMFE::Instance();

   TMFeError err = mfe->Connect(name.c_str(), __FILE__);
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *common = new TMFeCommon();
   common->EventID = 1;
   common->LogHistory = 1;
   //common->Buffer = "SYSTEM";

   TMFeEquipment* eq = new TMFeEquipment(mfe, name.c_str(), common);
   eq->Init();
   eq->SetStatus("Starting...", "white");
   eq->ZeroStatistics();
   eq->WriteStatistics();

   mfe->RegisterEquipment(eq);

   feLabview* myfe = new feLabview(mfe, eq);
   cout << "myfe = " << myfe << endl;
   mfe->RegisterRpcHandler(myfe);

   //mfe->SetTransitionSequenceStart(910);
   //mfe->SetTransitionSequenceStop(90);
   //mfe->DeregisterTransitionPause();
   //mfe->DeregisterTransitionResume();

   myfe->Init();
   bool connected = myfe->TCPConnect();
   if(connected) connected = myfe->Handshake();
   if(!connected){
      cm_msg(MERROR, "TCPConnect", "Could not connect to host: %s:%s", myfe->fHostname.c_str(), myfe->fPortnum.c_str());
   }

   char rot_set_str[80];
   HNDLE hkey;
   sprintf(rot_set_str, "/Equipment/%s/Settings/Rotation_position", name.c_str());
   int status = db_find_key(mfe->fDB, 0, rot_set_str, &hkey);
   if (status != DB_SUCCESS) {
      cm_msg(MERROR, "Init", "Key not found: %s", rot_set_str);
   }
   db_watch(mfe->fDB, hkey, callback, (void*)myfe);

   mfe->RegisterPeriodicHandler(eq, myfe);

   eq->SetStatus("Started...", "white");

   while (!mfe->fShutdownRequested) {
      mfe->PollMidas(10);
   }

   mfe->Disconnect();

   return 0;
}

/* emacs
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 3
 * indent-tabs-mode: nil
 * End:
 */
