#ifndef _LABVIEWDRIVER_H_
#define _LABVIEWDRIVER_H__

typedef struct {
  char      hostname;
  char      port;
  BOOL      dynamic;
  char      Rot_position[32];
  char      Trans_position[32];
  char      LabVar[12][32];
  char      names[12][32];
} LABVIEWDRIVER_SETTINGS;

#define LABVIEWDRIVER_SETTINGS_STR(_name) const char *_name[] = {\
"[.]",\
"hostname = STRING [32] 142.90.101.189",\
"port = STRING [32] 2000",\
"Dynamic_mode = BOOL : y",\
"Rotation_position = STRING [32] 100",\
"Translation_position = STRING [32] 100",\
"LabV_monitored_variables = STRING[12] :",\
"[32] var1",\
"[32] var2",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"Names = STRING[12] :",\
"[32] var1",\
"[32] var2",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"[32] ",\
"",\
NULL }


typedef struct {
  float       LabVarValue[12];
} LABVIEWDRIVER_VARIABLES;


#define LABVIEWDRIVER_VARIABLES_STR(_name) const char *_name[] = {\
"[.]",\
"LabV_var_val = FLOAT[12] :",\
"[0] ",\
"[1] ",\
"[2] ",\
"[3] ",\
"[4] ",\
"[5] ",\
"[6] ",\
"[7] ",\
"[8] ",\
"[9] ",\
"[10] ",\
"[11] ",\
"",\
NULL }


#endif
