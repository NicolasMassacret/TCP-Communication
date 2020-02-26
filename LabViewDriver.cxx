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
#include <algorithm>
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
#define SEPARATOR "_"

vector<string> split(const string& str, const string& delim)
{
    vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

int add_key(HNDLE hDB, HNDLE hkey, KEY *key, INT level, void *pvector){
   if(key->type != TID_KEY)
      ((vector<KEY>*)pvector)->push_back(*key);
   return DB_SUCCESS;
}

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
      // fEventSize = 100;
      // fEq->fOdbEqSettings->RI("event_size", &fEventSize, true);
      // if (fEventBuf) {
      //    free(fEventBuf);
      // }
      // fEventBuf = (char*)malloc(fEventSize);

      // add ODB settings that are internal -> not for LabView
      sets.push_back("hostname");
      stype.push_back(TID_STRING);
      sets.push_back("port");
      stype.push_back(TID_STRING);

      nFixedSettings = sets.size();
      nFixedVars = vars.size();

      // settings.LabVar.resize(NCH);
      // settings.names.resize(NCH);
      // variables.resize(NCH);
      // fEq->fOdbEqSettings->RSA("LabV_monitored_variables", &settings.LabVar, true, NCH);
      // fEq->fOdbEqSettings->RB("Dynamic_mode", &settings.dynamic, true);
      // fEq->fOdbEqSettings->RF("Rotation_position", &settings.Rot_position, true);
      // fEq->fOdbEqSettings->RF("Translation_position", &settings.Trans_position, true);
      // fEq->fOdbEqVariables->RDA("LabV_var_val", &variables, true, NCH);

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
      string resp = Exchange("midas\r\n");
      bool correct = (resp.substr(0,7) == string("labview"));
      if(!correct) fMfe->Msg(MERROR, "Handshake", "Unexpected response: %s", resp.c_str());
      else if(verbose) fMfe->Msg(MINFO, "Handshake", "Handshake successful");
      return correct;
   }

   unsigned int GetVars();
private:
   vector<string> vars, sets;
   vector<int> vtype, stype;
   unsigned int nFixedSettings, nFixedVars;
   int verbose = 1;
};

void callback(INT hDB, INT hkey, INT index, void *feptr)
{
   feLabview* fe = (feLabview*)feptr;
   fe->fecallback(hDB, hkey, index);
}

unsigned int feLabview::GetVars()
{
   sets.resize(nFixedSettings); vars.resize(nFixedVars);
   stype.resize(nFixedSettings); vtype.resize(nFixedVars);
   string resp = Exchange("list_vars\r\n");
   cout << "Response: " << resp << "(" << resp.size() << ")" << endl;
   vector<string> tokens = split(resp, SEPARATOR);
   for(string s: tokens){
      char set_or_var = s[0];
      char ctype = s[1];
      s.erase(0,2);
      int type;
      switch(ctype){
      case 'B': type = TID_BOOL; break;
      case 'I': type = TID_INT; break;
      case 'F': type = TID_FLOAT; break;
      case 'D': type = TID_DOUBLE; break;
      case 'S': type = TID_STRING; break;
      case 'u': type = TID_WORD; break;
      case 'U': type = TID_DWORD; break;
      default : type = 0; fMfe->Msg(MERROR, "GetVars", "Unsupported data type: %c",  ctype);
      }
      if(set_or_var == 'W'){
         sets.push_back(s);
         stype.push_back(type);
      } else if(set_or_var == 'R'){
         vars.push_back(s);
         vtype.push_back(type);
      }
   }
   vector<string> odbsets, odbvars;
   vector<int> odbstid, odbvtid;
   vector<KEY> odbsetkeys, odbvarkeys;
   char tmpbuf[80];
   sprintf(tmpbuf, "/Equipment/%s/Settings", fMfe->fFrontendName.c_str());
   HNDLE odbs, odbv;
   db_find_key(fMfe->fDB, 0, tmpbuf, &odbs);
   sprintf(tmpbuf, "/Equipment/%s/Variables", fMfe->fFrontendName.c_str());
   db_find_key(fMfe->fDB, 0, tmpbuf, &odbv);
   db_scan_tree(fMfe->fDB, odbs, 0, add_key, (void*)&odbsetkeys);
   db_scan_tree(fMfe->fDB, odbv, 0, add_key, (void*)&odbvarkeys);
   for(KEY key: odbsetkeys){
      odbsets.push_back(key.name);
      odbstid.push_back(key.type);
   }
   for(KEY key: odbvarkeys){
      odbvars.push_back(key.name);
      odbvtid.push_back(key.type);
   }
   // fEq->fOdbEqSettings->ReadDir(&odbsets, &odbstid, &odbsnum, &tsize, &isize); // FIXME: function not implememnted!
   // fEq->fOdbEqVariables->ReadDir(&odbvars, &odbvtid, &odbvnum, &tsize, &isize);
   if(verbose > 1){
      for(unsigned int i = 0; i < odbsets.size(); i++){
         cout << odbsets[i] << '\t' << odbstid[i] << endl;
      }
      for(unsigned int i = 0; i < odbvars.size(); i++){
         cout << odbvars[i] << '\t' << odbvtid[i] << endl;
      }
   }
   if(verbose){
      // if(odbsets == sets) cout << "Settings match!" << endl;
      // else {
         cout << "Settings don't match!" << endl;
         cout << "LabView:\t";
         for(string s: sets) cout << s << '\t';
         cout <<  endl;
         cout << "ODB:    \t";
         for(string s: odbsets) cout << s << '\t';
         cout <<  endl;
      // }
   }
   for(unsigned int i = 0; i < sets.size(); i++){
      bool found = false;
      if(!sets[i].size())
         cerr << "Empty sets string at pos " << i << endl;
      vector<string>::iterator it = std::find(odbsets.begin(), odbsets.end(), sets[i]);
      if(it != odbsets.end()){
         unsigned int j = distance(odbsets.begin(), it);
         if(stype[i] == odbstid[j]) found = true;
         else {
            fMfe->Msg(MERROR, "GetVars", "Key %s exists, but has wrong type: %d instead of %d. Delete key manually to generate correct type.", sets[i].c_str(), odbstid[j], stype[i]);
            exit(DB_TYPE_MISMATCH);
            // don't want to delete keys automatically
         }
      }
      if(!found){
         db_create_key(fMfe->fDB, odbs, sets[i].c_str(), stype[i]);
      }
   }
   for(unsigned int i = 0; i < vars.size(); i++){
      bool found = false;
      if(!vars[i].size())
         cerr << "Empty vars string at pos " << i << endl;
      vector<string>::iterator it = std::find(odbvars.begin(), odbvars.end(), vars[i]);
      if(it != odbvars.end()){
         unsigned int j = distance(odbvars.begin(), it);
         if(vtype[i] == odbvtid[j]) found = true;
         else {
            fMfe->Msg(MERROR, "GetVars", "Key %s exists, but has wrong type: %d instead of %d. Delete key manually to generate correct type.", vars[i].c_str(), odbvtid[j], vtype[i]);
            exit(DB_TYPE_MISMATCH);
            // don't want to delete keys automatically
         }
      }
      if(!found){
         db_create_key(fMfe->fDB, odbv, vars[i].c_str(), vtype[i]);
      }
   }
   int orphans = 0;
   for(unsigned int i = nFixedSettings; i < odbsets.size(); i++){
      vector<string>::iterator it = std::find(sets.begin(), sets.end(), odbsets[i]);
      if(it == sets.end()){
         orphans++;
         fMfe->Msg(MINFO, "GetVars", "Orphaned key: Setting %s does not match available LabView settings", odbsets[i].c_str());
         // message
      } else {
         // create hotlink
         HNDLE hkey;
         int status = db_find_key(fMfe->fDB, odbs, sets[i].c_str(), &hkey);
         if (status != DB_SUCCESS) {
            cm_msg(MERROR, "Init", "Key not found: %s", sets[i].c_str());
         }
         db_watch(fMfe->fDB, hkey, callback, (void*)this);
      }
   }
   for(string s: odbvars){
      vector<string>::iterator it = std::find(vars.begin(), vars.end(), s);
      if(it == vars.end()){
         orphans++;
         fMfe->Msg(MINFO, "GetVars", "Orphaned key: Variable %s does not match available LabView variables", s.c_str());
         // message
      }
   }
   if(orphans)
      fMfe->Msg(MINFO, "GetVars", "Orphaned keys in ODB found: %d", orphans);
   return tokens.size();
}

void feLabview::fecallback(HNDLE hDB, HNDLE hkey, INT index)
{
   KEY key;
   int status = db_get_key(hDB, hkey, &key);

   std::ostringstream oss, oss2;
   oss << 'W';
   oss2 << 'W';

   if(status == DB_SUCCESS){
      switch(key.type){
      case TID_BOOL:
         {
            bool val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            // sprintf(reqstr, (val?"true":"false"));
            oss << 'B' << key.name << SEPARATOR << int(val) << endl;
            oss2 << 'B' << key.name << SEPARATOR << '?' << endl;
            Exchange(oss.str(), false);
            string resp=Exchange(oss2.str());
            bool rbk = !val;
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               try{
                  rbk = bool(std::stoi(rv[1]));
               }
               catch (const std::invalid_argument& ia){
                  cm_msg(MERROR, "fecallback", "Received incompatible response from LabView: >%s<\n", resp.c_str());
               }
            }
            if(rbk != val){
               cm_msg(MERROR, "fecallback", "Readback for %s doesn't match request: %d != %d\n", key.name, int(rbk), int(val));
            }
            break;
         }
      case TID_INT:
         {
            int val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            // sprintf(reqstr, "%d", val);
            oss << 'I' << key.name << SEPARATOR << val << endl;
            oss2 << 'I' << key.name << SEPARATOR << '?' << endl;
            Exchange(oss.str(), false);
            string resp=Exchange(oss2.str());
            int rbk = -99999;
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               try{
                  rbk = std::stoi(rv[1]);
               }
               catch (const std::invalid_argument& ia){
                  cm_msg(MERROR, "fecallback", "Received incompatible response from LabView: >%s<\n", resp.c_str());
               }
            }
            if(rbk != val){
               cm_msg(MERROR, "fecallback", "Readback for %s doesn't match request: %d != %d\n", key.name, rbk, val);
            }
            break;
         }
      case TID_DOUBLE:
         {
            double val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            oss << 'D' << key.name << SEPARATOR << val << endl;
            oss2 << 'D' << key.name << SEPARATOR << '?' << endl;
            Exchange(oss.str(), false);
            string resp=Exchange(oss2.str());
            double rbk = -99999;
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               try{
                  rbk = std::stod(rv[1]);
               }
               catch (const std::invalid_argument& ia){
                  cm_msg(MERROR, "fecallback", "Received incompatible response from LabView: >%s<\n", resp.c_str());
               }
            }
            if(rbk != val){
               cm_msg(MERROR, "fecallback", "Readback for %s doesn't match request: %f != %f\n", key.name, rbk, val);
            }
            break;
         }
      case TID_FLOAT:
         {
            float val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            oss << 'F' << key.name << SEPARATOR << val << endl;
            oss2 << 'F' << key.name << SEPARATOR << '?' << endl;
            Exchange(oss.str(), false);
            string resp=Exchange(oss2.str());
            float rbk = -99999;
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               try{
                  rbk = std::stod(rv[1]);
               }
               catch (const std::invalid_argument& ia){
                  cm_msg(MERROR, "fecallback", "Received incompatible response from LabView: >%s<\n", resp.c_str());
               }
            }
            if(rbk != val){
               cm_msg(MERROR, "fecallback", "Readback for %s doesn't match request: %f != %f\n", key.name, rbk, val);
            }
            break;
         }
      case TID_STRING:
         {
            char val[256];
            int size = 256;
            db_get_data(hDB, hkey, (void*)val, &size, key.type);
            oss << 'S' << key.name << SEPARATOR << val << endl;
            oss2 << 'S' << key.name << SEPARATOR << '?' << endl;
            Exchange(oss.str(), false);
            string resp=Exchange(oss2.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[1] != string(val)){
                  cm_msg(MERROR, "fecallback", "Readback for %s doesn't match request: %s != %s\n", key.name, rv[1].c_str(), val);
               }
            }
            break;
         }
      case TID_WORD:
         {
            uint16_t val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            oss << 'B' << key.name << SEPARATOR << int(val);
            oss2 << 'B' << key.name << SEPARATOR << '?';
            Exchange(oss.str(), false);
            string resp=Exchange(oss2.str());
            uint16_t rbk = 0;
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               try{
                  rbk = std::stoi(rv[1]);
               }
               catch (const std::invalid_argument& ia){
                  cm_msg(MERROR, "fecallback", "Received incompatible response from LabView: >%s<\n", resp.c_str());
               }
            }
            if(rbk != val){
               cm_msg(MERROR, "fecallback", "Readback for %s doesn't match request: %d != %d\n", key.name, int(rbk), int(val));
            }
            // sprintf(reqstr, "%d", val);
            break;
         }
      case TID_DWORD:
         {
            uint32_t val;
            int size = sizeof(val);
            db_get_data(hDB, hkey, (void*)&val, &size, key.type);
            oss << 'B' << key.name << SEPARATOR << int(val) << endl;
            oss2 << 'B' << key.name << SEPARATOR << '?' << endl;
            Exchange(oss.str(), false);
            string resp=Exchange(oss2.str());
            uint32_t rbk = 0;
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               try{
                  rbk = std::stoi(rv[1]);
               }
               catch (const std::invalid_argument& ia){
                  cm_msg(MERROR, "fecallback", "Received incompatible response from LabView: >%s<\n", resp.c_str());
               }
            }
            if(rbk != val){
               cm_msg(MERROR, "fecallback", "Readback for %s doesn't match request: %d != %d\n", key.name, int(rbk), int(val));
            }
            break;
         }
      }
      cm_msg(MINFO, "callback", "Change requested: %s\n", oss.str().c_str());
   }
   // //char respond;
   // std::string respond_str(resp.c_str());

   // printf("Value returned: %s \n", respond_str.c_str());
}

// INT feLabview::read_event(char *pevent, INT off)
// INT feLabview::read_event()
// {
//    double value;

//    if(settings.dynamic==true)
//       {
//          /*Update variables to monitor and destination in ODB*/
//          fEq->fOdbEqSettings->RSA("LabV_monitored_variables", &settings.LabVar, true, NCH);
//       }

//    /*Read variables and write their value in ODB*/
//    for(unsigned int a=0; a<settings.LabVar.size(); a++)
//       {
//          if(settings.LabVar[a].size())
//             {
//                // printf("Reading variable : %s \n", settings.LabVar[a].c_str());
//                string resp=Exchange(string("read_")+settings.LabVar[a]+"\r\n");
//                try{
//                   value = std::stod(resp);
//                }
//                catch (const std::invalid_argument& ia){
//                   value=-99999;
//                   printf("Received empty message from client. \n");
//                }

//                printf("Value returned: %f \n", value);

//                variables[a]=value;
//             }
//       }
//    fEq->fOdbEqVariables->WDA("LabV_var_val", variables);

//    return 0;
// }

INT feLabview::read_event()
{
   for(unsigned int i = 0; i < vars.size(); i++){
      std::ostringstream oss;
      oss << 'R';
      switch(vtype[i]){
      case TID_BOOL:
         {
            bool val(false);
            oss << 'B' << vars[i] << "_?\r\n";
            string resp=Exchange(oss.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[0].substr(2) == vars[i]){
                  try{
                     val = bool(std::stoi(rv[1]));
                     if(verbose) cout << vars[i] << " = " << int(val) << endl;
                     fEq->fOdbEqVariables->WB(vars[i].c_str(), val);
                  }
                  catch (const std::invalid_argument& ia){
                     cm_msg(MERROR, "read_event", "Received incompatible response from LabView: >%s<\n", resp.c_str());
                  }
               } else {
                  cm_msg(MERROR, "read_event", "Received wrong variable from LabView: >%s<\n", resp.c_str());
               }
            }
            break;
         }
      case TID_INT:
         {
            int val;
            oss << 'I' << vars[i] << "_?\r\n";
            string resp=Exchange(oss.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[0].substr(2) == vars[i]){
                  try{
                     val = std::stoi(rv[1]);
                     if(verbose) cout << vars[i] << " = " << val << endl;
                     fEq->fOdbEqVariables->WI(vars[i].c_str(), val);
                  }
                  catch (const std::invalid_argument& ia){
                     cm_msg(MERROR, "read_event", "Received incompatible response from LabView: >%s<\n", resp.c_str());
                  }
               } else {
                  cm_msg(MERROR, "read_event", "Received wrong variable from LabView: >%s<\n", resp.c_str());
               }
            }
            break;
         }
      case TID_DOUBLE:
         {
            double val;
            oss << 'D' << vars[i] << "_?\r\n";
            string resp=Exchange(oss.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[0].substr(2) == vars[i]){
                  try{
                     val = std::stod(rv[1]);
                     if(verbose) cout << vars[i] << " = " << val << endl;
                     fEq->fOdbEqVariables->WD(vars[i].c_str(), val);
                  }
                  catch (const std::invalid_argument& ia){
                     cm_msg(MERROR, "read_event", "Received incompatible response from LabView: >%s<\n", resp.c_str());
                  }
               } else {
                  cm_msg(MERROR, "read_event", "Received wrong variable from LabView: >%s<\n", resp.c_str());
               }
            }
            break;
         }
      case TID_FLOAT:
         {
            float val;
            oss << 'F' << vars[i] << "_?\r\n";
            string resp=Exchange(oss.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[0].substr(2) == vars[i]){
                  try{
                     val = std::stof(rv[1]);
                     if(verbose) cout << vars[i] << " = " << val << endl;
                     fEq->fOdbEqVariables->WF(vars[i].c_str(), val);
                  }
                  catch (const std::invalid_argument& ia){
                     cm_msg(MERROR, "read_event", "Received incompatible response from LabView: >%s<\n", resp.c_str());
                  }
               } else {
                  cm_msg(MERROR, "read_event", "Received wrong variable from LabView: >%s<\n", resp.c_str());
               }
            }
            break;
         }
      case TID_STRING:
         {
            oss << 'S' << vars[i] << "_?\r\n";
            string resp=Exchange(oss.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[0].substr(2) == vars[i]){
                  if(verbose) cout << vars[i] << " = " << rv[1] << endl;
                  fEq->fOdbEqVariables->WS(vars[i].c_str(), rv[1].c_str(), rv[1].size());
               }
            }
            break;
         }
      case TID_WORD:
         {
            uint16_t val;
            oss << 'u' << vars[i] << "_?\r\n";
            string resp=Exchange(oss.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[0].substr(2) == vars[i]){
                  try{
                     val = std::stoi(rv[1]);
                     if(verbose) cout << vars[i] << " = " << val << endl;
                     fEq->fOdbEqVariables->WU16(vars[i].c_str(), val);
                  }
                  catch (const std::invalid_argument& ia){
                     cm_msg(MERROR, "read_event", "Received incompatible response from LabView: >%s<\n", resp.c_str());
                  }
               } else {
                  cm_msg(MERROR, "read_event", "Received wrong variable from LabView: >%s<\n", resp.c_str());
               }
            }
            break;
         }
      case TID_DWORD:
         {
            uint32_t val;
            oss << 'U' << vars[i] << "_?\r\n";
            string resp=Exchange(oss.str());
            vector<string> rv = split(resp, SEPARATOR);
            if(rv.size()==2){
               if(rv[0].substr(2) == vars[i]){
                  try{
                     val = std::stoi(rv[1]);
                     fEq->fOdbEqVariables->WU32(vars[i].c_str(), val);
                  }
                  catch (const std::invalid_argument& ia){
                     cm_msg(MERROR, "read_event", "Received incompatible response from LabView: >%s<\n", resp.c_str());
                  }
               } else {
                  cm_msg(MERROR, "read_event", "Received wrong variable from LabView: >%s<\n", resp.c_str());
               }
            }
            break;
         }
      }
   }
   return 0;
}

static void usage()
{
   fprintf(stderr, "Usage: LabViewDriver_tmfe.exe <name> ...\n");
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
   myfe->GetVars();

   // char rot_set_str[80];
   // HNDLE hkey;
   // sprintf(rot_set_str, "/Equipment/%s/Settings/Rotation_position", name.c_str());
   // int status = db_find_key(mfe->fDB, 0, rot_set_str, &hkey);
   // if (status != DB_SUCCESS) {
   //    cm_msg(MERROR, "Init", "Key not found: %s", rot_set_str);
   // }
   // db_watch(mfe->fDB, hkey, callback, (void*)myfe);

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
