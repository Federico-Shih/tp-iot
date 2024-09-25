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
#define TEMPERATURE_SENSOR AI0
#define HUMIDITY_SENSOR AI0
#define LEVEL_SENSOR AI0
#define FLUX_SENSOR DI0

float sensorRead[4];
#define TEMPERATURE_IDX 0
#define HUMIDITY_IDX 1
#define LEVEL_IDX 2
#define FLUX_IDX 3

float temperature = 0;
float humidity = 0;
float level = 0;
float flux = 0;

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
    sensorRead[TEMPERATURE_IDX] = analogRead(TEMPERATURE_SENSOR);
    sensorRead[HUMIDITY_IDX] = analogRead(HUMIDITY_SENSOR);
    sensorRead[LEVEL_IDX] = analogRead(LEVEL_SENSOR);

    temperature = (float(sensorRead[TEMPERATURE_IDX]) * 25 / 100) - 60;
    humidity = toPercentage(normalizeRead(toMilliAmpers(float(sensorRead[HUMIDITY_IDX]))));
    level = toPercentage((1 - normalizeRead(toMilliAmpers(float(sensorRead[LEVEL_IDX])))));
}

void readPulses() {
  Serial.print(digitalRead(FLUX_SENSOR));
}

void publishReport() {
    dtostrf(temperature, 4, 2, str);
    PublishData("Temperatura",str);

    dtostrf(humidity, 4, 2, str);
    PublishData("Humedad",str);

    dtostrf(level, 4, 2, str);
    PublishData("Nivel",str);
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