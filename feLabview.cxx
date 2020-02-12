//
// fetest_tmfe.cxx
//
// Frontend for test and example of tmfe c++ frontend
//

#include <stdio.h>
#include <signal.h> // SIGPIPE
#include <assert.h> // assert()
#include <stdlib.h> // malloc()
#include <string>
#include <iostream>
#include <sstream>
#include <algorithm>            // transform
#include <cctype>               // tolower
/// replace these with midas tcpip.o?
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "midas.h"
#include "tmfe.h"

#include "KOtcp.h"
#include "feTCP.h"

using std::string;
using std::vector;
using std::cout;
using std::cerr;
using std::endl;

#define NMOTORS 2
#define IZ 0
#define IPHI 1

class feLabview : public feTCP
{
public:
   feLabview(TMFE* mfe, TMFeEquipment* eq):feTCP(mfe, eq), fPositions(NMOTORS), fPositionSet(NMOTORS), fStartPos(NMOTORS), fProgress(NMOTORS, 1.), fMoving(NMOTORS,false){} // ctor

   void Init()
   {
      fEventSize = 100;
      fEq->fOdbEqSettings->RIA("Positions", &fPositionSet, true, NMOTORS);
      fEq->fOdbEqSettings->RIA("AccVel", &fAccvel, true, NMOTORS);
      fEq->fOdbEqSettings->RD("Zpos", &fzset, true);
      fEq->fOdbEqSettings->RD("Phi", &fphiset, true);
      fEq->fOdbEqVariables->RIA("Positions", &fPositions, true, NMOTORS);
      fEq->fOdbEqVariables->RFA("Progress", &fProgress, true, NMOTORS);
      fEq->fOdbEqVariables->RBA("Moving", &fMoving, true, NMOTORS);
      fEq->fOdbEqVariables->RD("Zpos", &fz, true);
      fEq->fOdbEqVariables->RD("Phi", &fphi, true);
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
      for(int i = 0; i < NMOTORS; i++){
         fPositions[i] = ReadPos(i+1);
         if(fMoving[i]){
            double totmove = fPositionSet[i]-fStartPos[i];
            if(totmove)
               fProgress[i] = double(fPositions[i]-fStartPos[i])/totmove;
            else
               fProgress[i] = 1.;
            fMoving[i] = (fPositions[i]!=fPositionSet[i]);
         }
      }
      fz = GetZ(fPositions[IZ]);
      fphi = GetPhi(fPositions[IPHI]);
      fEq->fOdbEqVariables->WIA("Positions", fPositions);
      fEq->fOdbEqVariables->WFA("Progress", fProgress);
      fEq->fOdbEqVariables->WBA("Moving", fMoving);
      fEq->fOdbEqVariables->WD("Zpos", fz);
      fEq->fOdbEqVariables->WD("Phi", fphi);

      fEq->fOdbEqSettings->RIA("Positions", &fPositionSet);
      fEq->fOdbEqSettings->RIA("AccVel", &fAccvel);

   }

   string HandleRpc(const char* cmd, const char* args)
   {
      fMfe->Msg(MINFO, "HandleRpc", "RPC cmd [%s], args [%s]", cmd, args);
      if (strcmp(cmd, "apply_change")==0) {
         // HandlePeriodic();      // make sure we have the latest information

         int index = atoi(args);
         string resp = SetM(index);
         fMfe->Msg(MINFO, "HandleRpc", "Response: %s", resp.c_str());
         return resp;
      } else {
         return "";
      }
   }
private:
   const double kZfactor = 1.;
   const double kZoffset = 0.;
   const double kPhifactor = 1.;
   const double kPhioffset = 0.;
   vector<int> fPositions, fPositionSet, fStartPos;
   double fzset, fphiset, fz, fphi;
   vector<int> fAccvel;
   vector<float> fProgress;
   vector<bool> fMoving;
   int ReadPos(int id){
      std::ostringstream oss;
      oss << 'm' << id;
      string resp = Exchange(string("read ")+oss.str()+"\r\n");
      if(resp.substr(0,6) == string("Error")){
         fMfe->Msg(MERROR, "ReadPos", "Response: %s", resp.c_str());
         return -1;
      }
      std::istringstream iss(resp);
      string idd;
      int pos;
      iss >> idd >> pos;
      if(tolower(idd)!=oss.str()){
         fMfe->Msg(MERROR, "ReadPos", "ID mismatch!: %s != %s", tolower(idd).c_str(), oss.str().c_str());
      }
      return pos;
   }

   string SetM(int index){
      if(index > NMOTORS) return "Bad index!";
      string response("");
      if(fPositionSet[index] != fPositions[index]){
         if(fMoving[index]){
            fMfe->Msg(MERROR, "SetM", "Motor %d is already moving!", index+1);
            return "Already moving!";
         }

         std::ostringstream resp;
         resp << "Set " << index;
         fStartPos[index] = fPositions[index];
         fMoving[index] = Set(index+1, fPositionSet[index], fAccvel[index]);
         if(fMoving[index]){
            resp << " OK";
         } else {
            resp << " Failed!";
         }
         response = resp.str();
         cout << "Set(" << index+1 << ", " << fPositionSet[index] << ", " << fAccvel[index] << ')' << endl;
      }
      return response;
   }

   bool Set(int id, int pos, int va){
      std::ostringstream oss, oss2;
      oss << "set m" << id << ' ' << pos << ' ' << va << "\r\n";
      oss2 << 'm' << id << " set " << pos << ' ' << va;
      string resp = Exchange(oss.str());
      cout << "Sent: " << oss.str() << endl;
      cout << "Recv: " << resp << endl;
      if(tolower(resp) == oss2.str()){
         return true;
      } else {
         fMfe->Msg(MERROR, "Set", "Response: >%s< != >%s<", resp.c_str(), oss2.str().c_str());
         return false;
      }
   }

   double GetZ(int pos){
      return kZfactor*pos+kZoffset;
   }
   double GetPhi(int pos){
      return kPhifactor*pos+kPhioffset;
   }

   string tolower(string data){
      std::transform(data.begin(), data.end(), data.begin(),
                     [](unsigned char c){ return std::tolower(c); });
      return data;
   }
};

static void usage()
{
   fprintf(stderr, "Usage: feLabview <name>\n");
   exit(1);
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

   TMFeError err = mfe->Connect("feLabview", __FILE__);
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *common = new TMFeCommon();
   common->EventID = 1;
   common->LogHistory = 1;
   //common->Buffer = "SYSTEM";

   TMFeEquipment* eq = new TMFeEquipment(mfe, "feLabview", common);
   eq->Init();
   eq->SetStatus("Starting...", "white");
   eq->ZeroStatistics();
   eq->WriteStatistics();

   mfe->RegisterEquipment(eq);

   feLabview* myfe = new feLabview(mfe, eq);

   mfe->RegisterRpcHandler(myfe);

   //mfe->SetTransitionSequenceStart(910);
   //mfe->SetTransitionSequenceStop(90);
   //mfe->DeregisterTransitionPause();
   //mfe->DeregisterTransitionResume();

   myfe->Init();

   mfe->RegisterPeriodicHandler(eq, myfe);

   eq->SetStatus("Started...", "white");

   bool connected = myfe->TCPConnect();

   if(connected){
      eq->SetStatus("Connected", "green");
      // string response = myfe->Exchange("Hi there!");
      // if(response.size()) mfe->Msg(MINFO, "TCP", "Response: %s", response.c_str());
      // else cout << "NO RESPONSE" << endl;
   } else {
      return 1;
   }

   while (!mfe->fShutdownRequested) {
      for(int i = 0; 1 < 10; i++) // 1 second
         mfe->PollMidas(100);
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
