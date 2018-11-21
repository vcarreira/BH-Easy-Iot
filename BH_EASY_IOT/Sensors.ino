#include <dht_nonblocking.h> // https://github.com/brunohorta82/DHT_nonblocking
#include <OneWire.h> // Install on Arduino IDE Library manager 

#define TIME_READINGS_DELAY 30000ul
#define SENSOR_DEVICE  "sensor"
#define SENSOR_PIN 14
#define TEMPERATURE_TYPE 1
#define HUMIDITY_TYPE 2
#define DS18B20_TYPE 90

JsonArray& sns = getJsonArray();

typedef struct {
    JsonObject& sensorJson;
    DHT_nonblocking* dht;
    OneWire* oneWire;
    DallasTemperature* dallas;  
} sensor_t;
std::vector<sensor_t> _sensors;

const String sensorsFilename = "sensors.json";

JsonArray& getStoredSensors(){
  return sns;
}

void sensorJson(String _id,int _gpio ,bool _disabled, String _icon, String _name,  int _type,JsonArray& functions){
      JsonObject& sensorJson = getJsonObject();
      sensorJson.set("id", _id);
      sensorJson.set("gpio", _gpio);
      sensorJson.set("icon", _icon);
      sensorJson.set("name", _name);
      sensorJson.set("disabled", _disabled);
      sensorJson.set("type", _type);
      sensorJson.set("class", SENSOR_DEVICE);
      sensorJson.set("functions", functions);
      sns.add(sensorJson);
   
}
void createDefaultSensors(){
    String id = "S1";
    JsonArray& functionsJson = getJsonArray();
    createFunctions(functionsJson,id,DS18B20_TYPE);
    sensorJson(id,SENSOR_PIN,DISABLE,  "fa-microchip","Temperature", DS18B20_TYPE,functionsJson);
   
}
void createFunctions( JsonArray& functionsJson,String id,int type){

  switch(type){
    case DS18B20_TYPE:
    createFunctionArray(functionsJson,"Temperatura","temperature","fa-thermometer-half","ºC",MQTT_STATE_TOPIC_BUILDER(id,SENSOR_DEVICE,"temperature"),false,TEMPERATURE_TYPE);
    break;
    case DHT_TYPE_11:
    case DHT_TYPE_21:
    case DHT_TYPE_22:
      createFunctionArray(functionsJson,"Temperatura","temperature","fa-thermometer-half","ºC",MQTT_STATE_TOPIC_BUILDER(id,SENSOR_DEVICE,"temperature"),false,TEMPERATURE_TYPE);
      createFunctionArray(functionsJson,"Humidade","humidity","fa-percent","%", MQTT_STATE_TOPIC_BUILDER(id,SENSOR_DEVICE,"humidity"),false, HUMIDITY_TYPE);
    break;
    }
 
  }
void createFunctionArray(JsonArray& functionsJson,String _name, String _uniqueName,String _icon, String _unit, String _mqttStateTopic, bool _retain, int _type ){
    JsonObject& functionJson = functionsJson.createNestedObject();
      functionJson.set("name", _name);
      functionJson.set("uniqueName", _uniqueName);
      functionJson.set("icon", _icon);
      functionJson.set("unit", _unit);
      functionJson.set("type", _type);
      functionJson.set("mqttStateTopic", _mqttStateTopic);
      functionJson.set("mqttRetain", _retain);
}
void loopSensors(){
    float temperature = 0; 
    float humidity = 0;
   static unsigned long measurement_timestamp = millis( );
   if( millis( ) - measurement_timestamp > TIME_READINGS_DELAY ){
    for (unsigned int i=0; i < _sensors.size(); i++) {
      if(_sensors[i].sensorJson.get<bool>("disabled")){
        continue;
        }
    switch(_sensors[i].sensorJson.get<unsigned int>("type")){
      case DS18B20_TYPE:
        _sensors[i].dallas->begin();
        _sensors[i].dallas->requestTemperatures();
        measurement_timestamp = millis( );
        temperature =   _sensors[i].dallas->getTempCByIndex(0);
        if( temperature == 85.0 ||  temperature == (-127.0)){
          continue;
          }
      break;
      case DHT_TYPE_11:
      case DHT_TYPE_21:
      case DHT_TYPE_22:
     if(!_sensors[i].dht->measure( &temperature, &humidity )){
      continue;
      }
      break;
      }
      logger("[SENSORS] "+String(temperature,1));
        measurement_timestamp = millis( );
        JsonArray& functions = _sensors[i].sensorJson.get<JsonVariant>("functions");
        for(int i  = 0 ; i < functions.size() ; i++){
          JsonObject& f = functions[i];    
          String _mqttState = f.get<String>("mqttStateTopic");
          int _type =f.get<unsigned int>("type");
          bool _retain =f.get<bool>("mqttRetain");   
          if(_type == TEMPERATURE_TYPE){
            publishOnMqttQueue(_mqttState ,String(  temperature,1),_retain);
          }else if(_type == HUMIDITY_TYPE){
            publishOnMqttQueue(_mqttState ,String( humidity,1),_retain);
          }
        }
    }
    }
}
JsonArray& saveSensor(String _id,JsonObject& _sensor){
  for (unsigned int i=0; i < sns.size(); i++) {
    JsonObject& sensorJson = sns.get<JsonVariant>(i);  
    if(sensorJson.get<String>("id").equals(_id)){
      
      String _name = _sensor.get<String>("name");
      sensorJson.set("gpio",_sensor.get<unsigned int>("gpio"));
      sensorJson.set("name",_name);
      sensorJson.set("discoveryDisabled",_sensor.get<bool>("discoveryDisabled"));
      sensorJson.set("disabled",_sensor.get<bool>("disabled"));
     removeComponentHaConfig(getConfigJson().get<String>("homeAssistantAutoDiscoveryPrefix"),getConfigJson().get<String>("nodeId"), _sensors[i].sensorJson.get<String>("type"), _sensors[i].sensorJson.get<String>("class"), _sensors[i].sensorJson.get<String>("id"));
      if(  sensorJson.get<unsigned int>("type") != _sensor.get<unsigned int>("type")){
          sensorJson.remove("functions");
           JsonArray& functionsJson = getJsonArray();
          sensorJson.set("type",_sensor.get<unsigned int>("type"));
          createFunctions(functionsJson,_sensors[i].sensorJson.get<String>("id"),_sensor.get<unsigned int>("type"));
          sensorJson.set("functions",functionsJson);  
        }
        JsonArray& functions = sensorJson.get<JsonVariant>("functions");
        JsonArray& functionsUpdated = _sensor.get<JsonVariant>("functions");
        for(int i  = 0 ; i < functions.size() ; i++){
          for(int i  = 0 ; i < functions.size() ; i++){
          JsonObject& f = functions[i];
          JsonObject& fu = functionsUpdated[i];
          if(f.get<String>("uniqueName").equals(fu.get<String>("uniqueName")) && fu.get<String>("name") != NULL && !fu.get<String>("name").equals("")){
            f.set("name",fu.get<String>("name"));
            }

          }
        }
    }
    return sns;
  }

  saveSensors();
  applyJsonSensors();
 if(getConfigJson().get<bool>("homeAssistantAutoDiscovery")){
    createHASensorComponent();  
   }
}
void saveSensors(){
   if(SPIFFS.begin()){
      logger("[SENSORS] Open "+sensorsFilename);
      File rFile = SPIFFS.open(sensorsFilename,"w+");
      if(!rFile){
        logger("[SENSORS] Open sensors file Error!");
      } else {
      sns.printTo(rFile);
      }
      rFile.close();
   }else{
     logger("[SENSORS] Open file system Error!");
  }
  SPIFFS.end();
  logger("[SENSORS] New sensors config loaded.");
}
void loadStoredSensors(){
  bool loadDefaults = false;
  if(SPIFFS.begin()){
    File cFile;   
    #ifdef FORMAT
    SPIFFS.remove(sensorsFilename);
    #endif
    if(SPIFFS.exists(sensorsFilename)){
      cFile = SPIFFS.open(sensorsFilename,"r+"); 
      if(!cFile){
        logger("[SENSORS] Create file Sensor Error!");
        return;
      }
        logger("[SENSORS] Read stored file config...");
        JsonArray &storedSensors = getJsonArray(cFile);
        if (!storedSensors.success() || storedSensors.size()==0) {
         logger("[SENSORS] Json file parse Error!");
          loadDefaults = true;
        }else{
          
          logger("[SENSORS] Apply stored file config...");
          for(int i = 0 ; i< storedSensors.size(); i++){
            sns.add(storedSensors.get<JsonVariant>(i));
            }
          applyJsonSensors();
        }
        
     }else{
        loadDefaults = true;
     }
    cFile.close();
     if(loadDefaults){
      logger("[SENSORS] Apply default config...");
      cFile = SPIFFS.open(sensorsFilename,"w+"); 
      createDefaultSensors();
      sns.printTo(cFile);
      applyJsonSensors();
       
      cFile.close();
      }
     
  }else{
     logger("[SWITCH] Open file system Error!");
  }
   SPIFFS.end(); 
   
}

void applyJsonSensors(){
  _sensors.clear();
  for(int i  = 0 ; i < sns.size() ; i++){ 
    JsonObject& sensorJson = sns.get<JsonVariant>(i);  
    int gpio= sensorJson.get<unsigned int>("gpio");
    int type= sensorJson.get<unsigned int>("type");
    switch(type){
      case DHT_TYPE_11:
      case DHT_TYPE_21:
      case DHT_TYPE_22:
      {
        DHT_nonblocking* dht_sensor = new DHT_nonblocking( gpio,type );
        _sensors.push_back({sensorJson, dht_sensor,NULL,NULL});
      }
      break;
      case DS18B20_TYPE:   
      OneWire* oneWire = new OneWire (SENSOR_PIN);
      DallasTemperature* sensors = new DallasTemperature(oneWire);
       _sensors.push_back({sensorJson, NULL ,oneWire,sensors});
     break;
     }
  }
}

void rebuildSensorsMqttTopics(){
      bool store = false;
      for(int i  = 0 ; i < sns.size() ; i++){ 
        store = true;
        JsonObject& sensorJson = sns.get<JsonVariant>(i);  
        JsonArray& functions = sensorJson.get<JsonVariant>("functions");
        for(int i  = 0 ; i < functions.size() ; i++){
          JsonObject& f = functions.get<JsonVariant>(i);
          String _mqttState =f.get<String>("mqttStateTopic");
          String id = sensorJson.get<String>("id");
          String uniqueName = f.get<String>("uniqueName");
          f.set("mqttStateTopic",MQTT_STATE_TOPIC_BUILDER(id,SENSOR_DEVICE,uniqueName));
        }     
    }
    if(store){
      saveSensors();
      if(getConfigJson().get<bool>("homeAssistantAutoDiscovery")){
        createHASensorComponent();  
      }
    }
  }
