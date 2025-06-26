# Group_1: Connecting Sensor Data to Thingsboard on the Cloud


## Physical Set up
Setting up ESP32 board With BMP280:
![image](https://github.com/user-attachments/assets/7b62f9cf-4e04-4998-bafc-b37cfd39db4c)

Following the Diagram:
- Wire VDD(BMP280) to 3v3 (ESP32)
- Wire SDA (BMP280) to P21 (ESP32)
- Wire GND(BMP280) to GND (ESP32)
- Wire SCL (BMP280) to P22 (ESP32)

We get the following:


![image](https://github.com/user-attachments/assets/7e15469a-1139-4039-8bc6-506299635d3c)
![image](https://github.com/user-attachments/assets/f5a0d1ad-caeb-4298-87d7-9d87b8c7c987)


## Software Set up

### Configuring Thingsboard

1. Create an account at Thingsboard.cloud
2. Head on to Entities > Devices
3. Add Device > Add New Device
4. Provide name
5. Click Add Button at the bottom
6. Once Created, Click on the created device name
7. Copy Access Token
<img width="1800" alt="image" src="https://github.com/user-attachments/assets/01d3f965-6e05-4b44-851e-f0564657b8d4" />

## Testing Connection to Sensor Data

1. Install Arduino IDE
2. Set up according to: https://github.com/Theara-Seng/IOT_ESP32
3. Connect ESP32 to laptop/pc
4. go to file > example > BMPTEST
5. Compile
6. Go to Serial Monitor
7. You should see live update of the sensor data (might have to configure the baud to the right number if you cannot see it yet.)

## Connecting Sensor Data to Thingsboard

1. Copy the code in Thingsboard.ino
2. Paste into Arduino IDE
3. Configure WIFI_SSID to your wifi name
4. Configure WIFI_PASSWORD to your wifi password
5. Configure TOKEN to your access token copied from Thingsboard earlier
6. Configure THINGSBOARD_SERVER to "thingsboard.cloud"
7. Compile

## Displaying Sensor Data on Thingsboard

1. Go to Thingsboard > Entities > Devices
2. Confirm that your Device is active after compiling the code in Arduino IDE
3. After confirming Device is Active, head over to Dashboards > Add Dashboards > Create New Dashboard > Add title > Add
4. Click on the dashboard created
5. Add new widget accordingly while making sure each widget's datasource is from your device

![image](https://github.com/user-attachments/assets/7a386e35-5492-4a47-a6ed-12fd07b8e2f5)


