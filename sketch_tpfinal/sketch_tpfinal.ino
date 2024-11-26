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

const char user_name[] = "qgDqQjzwbsOdlxq1WizY"; // aca se debe poner el id del device de tdata

// GENERADOR CORRIENTE
#define MAX_CC 20 // in mA
#define MIN_CC 4
#define RANGE_CC (MAX_CC - MIN_CC)


// SENSORES
#define ENERGY_SENSOR AI0
#define NATURAL_LIGHT_SENSOR AI0
#define MOVEMENT_SWITCH DI0

//SETUP CASO POTENCIA CONSUMIDA (kWh)
float voltage = 220.0; // Voltaje en voltios (V)
float current_min = 5.0; // 5 mA
float potency_min = 500.0; // 500 W para 5 mA
float scale_potency = 50.0; // 50 W por mA adicional
unsigned long SimulatedTime = 3600000;  // 1 hora en milisegundos (3600000 ms)
float current_power = 0;

//SETUP CASO LUX
#define LUX_MIN_CC 5 // in mA
#define LUX_MAX 1000 // lux

// SETUP CASO PRESENCIA
#define PRESSED 0
#define NOT_PRESSED 1
#define LED_DIM DO0
#define CCmin 5
#define CCmax 15
int pwmValue=0;

float sensorRead[3];
#define ENERGY_IDX 0       //energia consumida en kWh
#define NATURAL_LIGHT_IDX 1     // lux natural recibida
#define MOVEMENT_IDX 2  // estado de ocupacion del habitaculo

float energy = 0;
float natural_light = 0;
float artificial_light = 0;
float percentage_light = 0;
float movement = 0;
float energy_alarm = 0; //alertar sobre fallo en el sensor de potencia - Asociada con el valor 6mA + tolerancia
float movement_alarm = 0; //alertar sobre fallo en el sensor de movimiento - Asociada con el valor 8mA + tolerancia
float occupy_alarm = 0;
float tolerance = 0.20;
float cc_mA = 0;

// DATA KEYS IN T-DATA
#define ENERGY_KEY "energyConsumption" //kWh
#define CURRENT_KEY "currentPower" //kW
#define NATURAL_LIGHT_KEY "naturalLight"
#define ARTIFICIAL_LIGHT_KEY "artificialLight"
#define PERCENTAGE_LIGHT_KEY "percentageLight"
#define MOVEMENT_KEY "isOccupied"
#define ENERGY_ALARM_KEY "energySensorFailed"
#define MOVEMENT_ALARM_KEY "movementSensorFailed" //luces apagadas con presencia de personas
#define OCCUPY_ALARM_KEY "lightOnWithoutOccupy" // luces prendidas con desocupacion

char str[7];

int sensor_interval = 1000;
long sensor_time = 0; // in millis
int report_interval = 2000;
long report_time = 0; // in millis

void setup() 
{
  //analogReference(INTERNAL2V56);
  SerialMon.begin(115200);
  SerialMon.println(F("***********************************************************"));
  SerialMon.println(F(" Initializing Modem"));
  pinMode(SIM_SELECT,OUTPUT);
  pinMode(ENERGY_SENSOR, INPUT);
  pinMode(NATURAL_LIGHT_SENSOR, INPUT);
  pinMode(MOVEMENT_SWITCH, INPUT_PULLUP);
  pinMode(LED_DIM, OUTPUT);
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
  return value / 26.4;
}

float toPercentage(float value) {
  return value * 100;
}

float getPower(float current){  // in kWh
  return ((current - current_min) * scale_potency + potency_min)/1000 ;
} 



int getNaturalLux (float current){
 // int result = 100*(current - LUX_MIN_CC);
    int result = map(current, 5, 15, 1000, 0);
  if(result < 0) {return 0;}
  if(result > LUX_MAX) {return LUX_MAX;}
  return result;
}

void readSensors() {
    sensorRead[ENERGY_IDX] = analogRead(ENERGY_SENSOR);
    sensorRead[NATURAL_LIGHT_IDX] = analogRead(NATURAL_LIGHT_SENSOR);
    cc_mA = toMilliAmpers(sensorRead[ENERGY_IDX]);
    natural_light = getNaturalLux(toMilliAmpers(sensorRead[NATURAL_LIGHT_IDX]));
    artificial_light = LUX_MAX - natural_light;
    percentage_light = (artificial_light / LUX_MAX)*100;
    current_power = getPower(toMilliAmpers(sensorRead[ENERGY_IDX]));
    energy = current_power / 6;

    if (( cc_mA < (6 + tolerance)) && (cc_mA > (6 - tolerance))) {
          movement_alarm = 1;
    } else {movement_alarm = 0;}

    if ( cc_mA < (10 + tolerance) && cc_mA > (10 - tolerance)) {
          occupy_alarm = 1;
    } else {occupy_alarm = 0;}

 /*   if ((cc_mA < (8 + tolerance)) && cc_mA> (8 - tolerance)) {
          energy_alarm = 1;
    } else {energy_alarm = 0;}
*/
      // Escalar la corriente medida al rango PWM (0-255)
    pwmValue = map(cc_mA, CCmin, CCmax, 0, 255);
    // Limitar los valores de PWM al rango permitido
    //pwmValue = constrain(pwmValue, 0, 255);
    // Ajustar el brillo del LED
    analogWrite(LED_DIM, pwmValue);
}

void readPulses() {
  sensorRead[MOVEMENT_IDX] = digitalRead(MOVEMENT_SWITCH);
  movement = sensorRead[MOVEMENT_IDX] == PRESSED; //envio 1 si esta ocupado (presionado), 0 sino
}

void publishReport() {
    dtostrf(energy, 4, 2, str);
    PublishData(ENERGY_KEY, str);

    dtostrf(natural_light, 4, 2, str);
    PublishData(NATURAL_LIGHT_KEY, str);

    dtostrf(artificial_light, 4, 2, str);
    PublishData(ARTIFICIAL_LIGHT_KEY, str);

    dtostrf(percentage_light, 4, 2, str);
    PublishData(PERCENTAGE_LIGHT_KEY, str);

    dtostrf(movement, 4, 2, str);
    PublishData(MOVEMENT_KEY, str);

    dtostrf(movement_alarm, 4, 2, str);
    PublishData(MOVEMENT_ALARM_KEY, str);

    dtostrf(occupy_alarm, 4, 2, str);
    PublishData(OCCUPY_ALARM_KEY, str);

    dtostrf(energy_alarm, 4, 2, str);
    PublishData(ENERGY_ALARM_KEY, str);

    dtostrf(current_power, 4, 2, str);
    PublishData(CURRENT_KEY, str);
   
}

void loop() 
{
  if((millis() - sensor_time) > sensor_interval)
  {
    sensor_time = millis();

    readSensors();
    readPulses();

    
  }


  if((millis() - report_time) > report_interval)
  {
    report_time = millis();

    SerialMon.println(energy);
    SerialMon.println(current_power);
    SerialMon.println(movement);
    SerialMon.println(natural_light);
    SerialMon.println(artificial_light);
    SerialMon.println(percentage_light);
    SerialMon.println(cc_mA);
    SerialMon.println("********************************************************");

    publishReport();
  }

}




