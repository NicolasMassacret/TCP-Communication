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
using std::cout;
using std::cerr;
using std::endl;

static void usage()
{
   fprintf(stderr, "Usage: fetest_tmfe ...\n");
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

   TMFeError err = mfe->Connect("feTCP", __FILE__);
   if (err.error) {
      printf("Cannot connect, bye.\n");
      return 1;
   }

   //mfe->SetWatchdogSec(0);

   TMFeCommon *common = new TMFeCommon();
   common->EventID = 1;
   common->LogHistory = 1;
   //common->Buffer = "SYSTEM";

   TMFeEquipment* eq = new TMFeEquipment(mfe, "feTCP", common);
   eq->Init();
   eq->SetStatus("Starting...", "white");
   eq->ZeroStatistics();
   eq->WriteStatistics();

   mfe->RegisterEquipment(eq);

   feTCP* myfe = new feTCP(mfe, eq);

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
      string response = myfe->Exchange("Hi there!");
      if(response.size()) mfe->Msg(MINFO, "TCP", "Response: %s", response.c_str());
      else cout << "NO RESPONSE" << endl;
   }

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
