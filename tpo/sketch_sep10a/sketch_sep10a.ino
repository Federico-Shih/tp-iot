#include <RIC3D.h>
#include <RIC3DMODEM.h>


RIC3D device();

#define SerialMon Serial

#define SerialAT Serial3

// Module baud rate
uint32_t rate = 115200; 

// Select SIM Card (0 = right, 1 = left)
bool sim_selected = 1; 

const char apn[]      = "grupotesacom.claro.com.ar";
const char gprsUser[] = "";
const char gprsPass[] = "";

const char mqtt_ip[] = "10.25.1.152";
const char mqtt_port[] = "4090";

const char user_name[] = "BbLqC3W041ZSlg3arnFN"; // aca se debe poner el id del device de tdata

// GENERADOR CORRIENTE
#define MAX_CC 20 // in mA
#define MIN_CC 4
#define RANGE_CC (MAX_CC - MIN_CC)

// SENSORES
#define ELECTRICITY_SENSOR AI0
#define LIGHT_SENSOR AI0
#define MOVEMENT_SENSOR DI0

float sensorRead[4];
#define ELECTRICITY_IDX 0
#define LIGHT_IDX 1
//falta un idx 2
#define MOVEMENT_IDX 3

float electricity = 0;
float light = 0;
float movement = 0;

// DATA KEYS IN T-DATA
#define ELECTRICITY_KEY "energyConsumed"
#define LIGHT_KEY "naturalLight"
#define MOVEMENT_KEY "isOccupied"

char str[7];

int sensor_interval = 1000;
long sensor_time = 0; // in millis
int report_interval = 2000;
long report_time = 0; // in millis

void setup() 
{
  analogReference(INTERNAL2V56);
  SerialMon.begin(115200);
  SerialMon.println(F("***********************************************************"));
  SerialMon.println(F(" Initializing Modem"));
  pinMode(SIM_SELECT,OUTPUT);
  pinMode(FLUX_SENSOR,INPUT_PULLUP);
  digitalWrite(SIM_SELECT,sim_selected);
  SerialMon.print(" Sim selected is the one on the ");
  SerialMon.println(sim_selected?"left":"right");
  ModemBegin(&SerialAT,&SerialMon);
  ModemTurnOff();
  ModemTurnOn();
  SerialAT.begin(rate);
  SerialMon.println(" Opening MQTT service ");
  if(CreatePDPContext(apn, gprsUser,  gprsPass))
  {
    SerialMon.println("Error creating PDP context");
  }
  ActivatePDPContext();
  if(ConnectMQTTClient(user_name,mqtt_ip,mqtt_port))
  {
    SerialMon.println("Error connecting to tdata");
  }
}

float normalizeRead(float read) {
  return (read - MIN_CC) / RANGE_CC;
}

float toMilliAmpers(float value) {
  return value / 40;
}

float toPercentage(float value) {
  return value * 100;
}

void readSensors() {
    sensorRead[ELECTRICITY_IDX] = analogRead(ELECTRICITY_SENSOR);
    sensorRead[LIGHT_IDX] = analogRead(LIGHT_SENSOR);

    electricity = float(sensorRead[ELECTRICITY_IDX]);
    light = toPercentage(normalizeRead(toMilliAmpers(float(sensorRead[LIGHT_IDX]))));
}

void readPulses() {
  sensorRead[MOVEMENT_IDX] = digitalRead(MOVEMENT_SENSOR);

  movement = sensorRead[MOVEMENT_IDX];
}

void publishReport() {
    dtostrf(electricity, 4, 2, str);
    PublishData(ELECTRICITY_KEY, str);

    dtostrf(light, 4, 2, str);
    PublishData(LIGHT_KEY, str);

    dtostrf(movement, 4, 2, str);
    PublishData(MOVEMENT_KEY, str);
}

void loop() 
{
  if((millis() - sensor_time) > sensor_interval)
  {
    sensor_time = millis();

    readSensors();
    readPulses();

    //SerialMon.println(temperature);
    //SerialMon.println(humidity);
    //SerialMon.println(level);
  }


  if((millis() - report_time) > report_interval)
  {
    report_time = millis();

    publishReport();
  }
}