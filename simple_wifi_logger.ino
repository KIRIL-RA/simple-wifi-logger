#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SPI.h>
#include <SD.h>

#define F_NAME_MAIN "index.html"
#define F_NAME_CSS  "style.css"
#define F_NAME_LOAD_OPTIONS "parametrs.txt"

File myFile;

const int inputs[] = {A0};

const int size_fields = 1; // For name_state_field

const String log_file_directory = "log.dir/";
const String log_file_name_ = "log_";
const String log_file_expansion = ".csv";

const char symbol_output_field = '%'; // NOTE: DO NOT USE THIS SYMBOL OTHERWISE
const char *ssid = "Your SSID";
const char *password = "Your Password";

int error_code = 0;
int time_now;
int time_last = 0;
int log_file_version = 0;
int time_make_file_s = 10; // Time time after which create new log file 

String name_state_field[size_fields][2] = {{"STATE", "0"}}; // When changing the number of elements, you need to change inputs

String log_file_name = "";
String last_log_file_name = "";

ESP8266WebServer server(80);

void sendErrorMessage_(); // ONLY FOR server.onNotFound
void sendErrorMessage(String file_name);
void readGPIOState();
void fileLogic();
void loadFiles();

void setup(void) {

  // Initialize inputs
  pinMode(A0, INPUT);
  
  // Start Serial
  Serial.begin(115200);
  delay(2000);
  
  // SD initializing
  Serial.println("");
  Serial.print("INITIALIZATION SD: ");
  
  // If SD initialization failed, do not load device
  if(!SD.begin(D2)) Serial.print("FAILED, REBOOT DEVICE");
  
  // If SD initialized continue booting
  else {  
    Serial.println("DONE");
    Serial.print("SSID: ");
    Serial.println(ssid);
    Serial.print("PASSWORD: ");
    Serial.println(password);
      
    Serial.print("CONNECTION"); 
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
  
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {     
      delay(500);
      Serial.print(".");
    }

    // Print information about connection
    Serial.println("");
    Serial.println("DONE"); 
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    if (MDNS.begin("esp8266")) {
      Serial.println("MDNS responder started");
    }

    loadFiles();  
  
    server.onNotFound(sendErrorMessage_);
    server.begin();
    Serial.println("HTTP server started");
}}

void loop(void) {
  time_now =  millis()/1000;
  
  if(time_now > time_last) readGPIOState();
  
  server.handleClient();
  MDNS.update();
  
  time_last = time_now;
}

void fileLogic(){  

  // Update version 
  if(time_now - (log_file_version+1)*time_make_file_s > 0) log_file_version++;

  // Make file name
  log_file_name = log_file_directory;
  log_file_name += log_file_name_;  
  
  last_log_file_name = log_file_name;
  last_log_file_name += log_file_version+1;
  last_log_file_name += log_file_expansion;
  
  log_file_name += log_file_version;
  log_file_name += log_file_expansion;
}

void readGPIOState(){
  fileLogic();
  
  myFile = SD.open(log_file_name, FILE_WRITE);
  String final_string_log = (String)time_now;
  final_string_log += ", ";
  
  // Read input state and write to array and to file
  for(int input_number = 0; input_number < size_fields; input_number++){ 
    String readed_state = (String)analogRead(inputs[input_number]);
    
    name_state_field[input_number][1] = readed_state;    
    final_string_log += readed_state;    
    if(input_number+1 < size_fields) final_string_log += ", "; // To create the correct output file
  }
  
  myFile.println(final_string_log); 
  myFile.close();
}

void loadFiles(){
  error_code = 0;
  
  // Load log file
  server.on("/log.csv", []() {   
    myFile = SD.open(last_log_file_name);
    
    // If file opened
    if (myFile) {
      
      String log_;
      
      // Read file, make line 
      while (myFile.available()) {
        log_ += (char)myFile.read();
       }
       
       // copy line to final const line
       const String log_final = log_;

       // Send file 
       server.send(200, "text/csv", log_final);
       
       // Close the file:
       myFile.close();
      }
      
      // If the file did not open
      else {     
        error_code = 2;     
        sendErrorMessage(log_file_name);
  }});

  // Load main page
  server.on("/", []() {  
    myFile = SD.open(F_NAME_MAIN);

    // If file opened
    if (myFile) {
      
      String main_page;

      // Parsing the file character by character - searching for the character of the value output field, make line
      while (myFile.available()) {
        
        char symb_buffer = myFile.read();

        // If the symbol is found
        if(symb_buffer == symbol_output_field) {
          
          String str_buffer;

          // Re-reading the symbol 
          symb_buffer = myFile.read();

          // Until we reached the end
          while(symb_buffer != symbol_output_field){
            
            str_buffer += symb_buffer;
            symb_buffer = myFile.read();
          }

          // Writing values ​​to a page
          for(String name_state_field_buffer[2] : name_state_field)
          // Compare
          if(str_buffer == name_state_field_buffer[0]) main_page += name_state_field_buffer[1];
        }      

        // If it's not the symbol of output field
        else  main_page += symb_buffer;
    }

    // Make final line 
    const String main_page_final = main_page;

    // Send file
    server.send(200, "text/html", main_page_final);
    
    // Close the file:
    myFile.close();
  } 
  
  // If the file did not open
  else {    
    error_code = 2;
    sendErrorMessage(F_NAME_MAIN);
  }});

  // Load stylesheet
  server.on("/style.css", []() {
    myFile = SD.open(F_NAME_CSS);

    // If file opened
    if (myFile) {
      String css;

      // Read file and make line
      while (myFile.available()) {      
        css += (char)myFile.read();
      }

      // Make final line
      const String css_final = css;

      // Send file
      server.send(200, "text/css", css_final);
      
      // Close the file:
      myFile.close();
  }
  
  // If the file did not open
  else {    
    error_code = 2;
    sendErrorMessage(F_NAME_CSS);
  }});
  
}

void sendErrorMessage_() {
  sendErrorMessage("");
}
  
void sendErrorMessage(String file_name) {
  String message;
  
  // Generate error message
  switch (error_code){
    case 0:
    message += "PAGE NOT FOUND";
    break;
    
    case 2:
    message += "CAN'T OPEN FILE \n\nTarget file:";
    message += file_name;
    break;
  }

  // Send message
  server.send(404, "text/plain", message);
}
