Serial myPort = null;

long connected = 0;
int timeout = 5000;

final char YAW = 'Y';
final char PITCH = 'P';
final char ROLL = 'R';
final char BATT = 'V';
final char ALT = 'A';
final char TEMP = 'T';
final char HEADING = 'H';


void serialSetup(int comm_port) {
  if (comm_port == -1) {  serialTest(); return;}
  if (comm_port == -2) {
    serial_conn.clear();
    serial_conn.addItem("Refresh list", -2);
    serial_conn.addItems(Serial.list());
    serial_conn.addItem("Attempt to auto-connect", -1);
    return;
  }
  try {
    if (myPort != null) {  myPort.stop();}
    myPort = new Serial(this, Serial.list()[comm_port], 115200);
    myPort.clear();
    myPort.bufferUntil('\n');
    println("Serial started on: " + Serial.list()[comm_port]);
    myPort.write(0);
    myPort.write(0);
    myPort.write(0);
    myPort.write('B');
  }
  catch (Exception serialException) {
    myPort = null;
    println("Not Connected to: " + Serial.list()[comm_port]);
  }
}


void serialEvent(Serial myPort) {
  String serial_data;
  try {
    while (myPort.available() != 0) {
      connected = millis();
      serial_data = new String(myPort.readBytesUntil('\n'));
      //printConsole(serial_data);
      //serial_data = "l123";
      
      switch (serial_data.charAt(0)) {
        case (ROLL): {  roll = float(serial_data.substring(1)); break;}
        case (PITCH): {  pitch = float(serial_data.substring(1)); break;}
        case (YAW): {  yaw = -float(serial_data.substring(1)); break;}
        case (BATT): {  batt_voltage = float(serial_data.substring(1)); break;}
        case (ALT): {  altitude = float(serial_data.substring(1)); break;}
        case (TEMP): {  temperature = float(serial_data.substring(1)); break;}
        //case (HEADING): {  temperature = float(serial_data.substring(1)); break;}
        case('\n'): {break;}
        default: {  printConsole(serial_data); break;}//println(serial_data); break;}
      }
    }
  }
  catch (Exception serialException) {
    println("SerialEvent error!");
  }
}


void checkConn() {
  if (millis() - connected > timeout || millis() < timeout) {
    connected = 0;
    serial_conn.setColorBackground(color(219,39,22))
               .setLabel("Select Serial Port")
               ;
    //myPort = null;
  }
  else {
    serial_conn.setColorBackground(color(2,198,23))
               .setLabel("Connected")
               ;
  }
}

void serialTest() {
  for (int i = Serial.list().length - 1; i >= 0 && connected == 0; i--) {
    try {
      myPort = new Serial(this, Serial.list()[i], 115200);
      myPort.bufferUntil('\n');
      println("Serial started on: " + Serial.list()[i]);
      
      myPort.write('|');
      long start = millis();
      while (millis() - start < timeout && connected == 0) {
        delay(1);
      }
    }
    catch (Exception serialException) {
      println("Not Connected to: " + Serial.list()[i]);
      myPort = null;
    }
  }
}
