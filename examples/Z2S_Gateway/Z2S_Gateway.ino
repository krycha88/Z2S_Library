#pragma once

#define Z2S_GATEWAY

#include <ZigbeeGateway.h>

#include "esp_coexist.h"

#include <SuplaDevice.h>
#include <priv_auth_data.h>
#include "Z2S_Devices_Database.h"
#include "Z2S_Devices_Table.h"

#include <supla/network/esp_wifi.h>
#include <supla/device/supla_ca_cert.h>
#include <supla/storage/eeprom.h>
#include <supla/storage/littlefs_config.h>

#undef USE_WEB_INTERFACE
#define USE_WEB_INTERFACE

#ifdef USE_WEB_INTERFACE

#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/wifi_parameters.h>

Supla::EspWebServer                       suplaServer;

Supla::Html::DeviceInfo                   htmlDeviceInfo(&SuplaDevice);
Supla::Html::WifiParameters               htmlWifi;
Supla::Html::ProtocolParameters           htmlProto;

#endif


#define GATEWAY_ENDPOINT_NUMBER 1

#define BUTTON_PIN                  9  //Boot button for C6/H2



ZigbeeGateway zbGateway = ZigbeeGateway(GATEWAY_ENDPOINT_NUMBER);

Supla::Eeprom             eeprom;
Supla::ESPWifi            wifi; //(SUPLA_WIFI_SSID, SUPLA_WIFI_SSID);
Supla::LittleFsConfig     configSupla;

uint32_t startTime = 0;
uint32_t printTime = 0;
uint32_t zbInit_delay = 0;

bool zbInit = true;

void setup() {
  
  log_i("setup start");

  pinMode(BUTTON_PIN, INPUT);

  eeprom.setStateSavePeriod(5000);

  Supla::Storage::Init();

  auto cfg = Supla::Storage::ConfigInstance();

  cfg->commit();

#ifndef USE_WEB_INTERFACE

  log_i("undef webinterface");
  cfg->setGUID(GUID);
  cfg->setAuthKey(AUTHKEY);
  cfg->setWiFiSSID(SUPLA_WIFI_SSID);
  cfg->setWiFiPassword(SUPLA_WIFI_PASS);
  cfg->setSuplaServer(SUPLA_SVR);
  cfg->setEmail(SUPLA_EMAIL);

#endif

  Z2S_loadDevicesTable();

  Z2S_initSuplaChannels();

  //  Zigbee Gateway notifications

  zbGateway.onTemperatureReceive(Z2S_onTemperatureReceive);
  zbGateway.onHumidityReceive(Z2S_onHumidityReceive);
  zbGateway.onOnOffReceive(Z2S_onOnOffReceive);
  zbGateway.onRMSVoltageReceive(Z2S_onRMSVoltageReceive);
  zbGateway.onRMSCurrentReceive(Z2S_onRMSCurrentReceive);
  zbGateway.onRMSActivePowerReceive(Z2S_onRMSActivePowerReceive);
  zbGateway.onBatteryPercentageReceive(Z2S_onBatteryPercentageReceive);

  zbGateway.onIASzoneStatusChangeNotification(Z2S_onIASzoneStatusChangeNotification);

  zbGateway.onBoundDevice(Z2S_onBoundDevice);
  zbGateway.onBTCBoundDevice(Z2S_onBTCBoundDevice);

  zbGateway.setManufacturerAndModel("Supla", "Z2SGateway");
  zbGateway.allowMultipleBinding(true);

  Zigbee.addEndpoint(&zbGateway);

  //Open network for 180 seconds after boot
  Zigbee.setRebootOpenNetwork(180);

  //Supla
  
  SuplaDevice.setSuplaCACert(suplaCACert);
  SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);
  
  SuplaDevice.setName("Zigbee to Supla");
  //wifi.enableSSL(true);

  log_i("before SuplaDevice begin");
  SuplaDevice.begin();      
  
  startTime = millis();
  printTime = millis();
  zbInit_delay = millis();
}

zb_device_params_t *gateway_device;
zb_device_params_t *joined_device;

char zbd_model_name[32];
char zbd_manuf_name[32];

void loop() {
  
  SuplaDevice.iterate();

  /*if (millis() - printTime > 10000) {
    if (zbGateway.getGatewayDevices().size() > 0 ) {
      if (esp_zb_is_started() && esp_zb_lock_acquire(portMAX_DELAY)) {
        zb_device_params_t *gt_device = zbGateway.getGatewayDevices().front();
	      log_i("short address before 0x%x",gt_device->short_addr);
        gt_device->short_addr = esp_zb_address_short_by_ieee(gt_device->ieee_addr);
        log_i("short address after 0x%x",gt_device->short_addr);
        //zbGateway.sendAttributeWrite(gt_device, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID,
          //                          ESP_ZB_ZCL_ATTR_TYPE_U64,8, gt_device->ieee_addr);
        //zbGateway.sendIASzoneEnrollResponseCmd(gt_device, ESP_ZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS, 120);
        //zbGateway.sendAttributeRead(gt_device, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID);
      }
   esp_zb_lock_release();
    printTime = millis();
    }
  }*/
  //if (zbInit && wifi.isReady()) {
    if (zbInit && SuplaDevice.getCurrentStatus() == STATUS_REGISTERED_AND_READY) {
  
    Serial.println("zbInit");
    
    esp_coex_wifi_i154_enable();
  
    if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
      Serial.println("Zigbee failed to start!");
      Serial.println("Rebooting...");
      ESP.restart();
    }
    zbInit = false;
    startTime = millis();
 }
  
  if (digitalRead(BUTTON_PIN) == LOW) {  // Push button pressed
    // Key debounce handling
    delay(100);
    
    while (digitalRead(BUTTON_PIN) == LOW) {
      delay(50);
      if ((millis() - startTime) > 5000) {
        // If key pressed for more than 5 secs, factory reset Zigbee and reboot
        Serial.printf("Resetting Zigbee to factory settings, reboot.\n");
        Zigbee.factoryReset();
      }
    }
    Zigbee.openNetwork(180);
  }
  delay(100);

  if (zbGateway.isNewDeviceJoined()) {

    zbGateway.clearNewDeviceJoined();
    zbGateway.printJoinedDevices();

    while (!zbGateway.getJoinedDevices().empty())
    {
      joined_device = zbGateway.getLastJoinedDevice();
      
      strcpy(zbd_manuf_name,zbGateway.readManufacturer(joined_device->endpoint, joined_device->short_addr, joined_device->ieee_addr));
      log_i("manufacturer %s ", zbd_manuf_name);
      strcpy(zbd_model_name,zbGateway.readModel(joined_device->endpoint, joined_device->short_addr, joined_device->ieee_addr));
      log_i("model %s ", zbd_model_name);

      uint16_t devices_list_table_size = sizeof(Z2S_DEVICES_LIST)/sizeof(Z2S_DEVICES_LIST[0]);
      uint16_t devices_desc_table_size = sizeof(Z2S_DEVICES_DESC)/sizeof(Z2S_DEVICES_DESC[0]);
      bool device_recognized = false;

          for (int i = 0; i < devices_list_table_size; i++) {
            
            if ((strcmp(zbd_model_name, Z2S_DEVICES_LIST[i].model_name) == 0) &&
            (strcmp(zbd_manuf_name, Z2S_DEVICES_LIST[i].manufacturer_name) == 0)) {
              log_i(  "LIST matched %s::%s, entry # %d, endpoints # %d, endpoints 0x%x::0x%x,0x%x::0x%x,0x%x::0x%x,0x%x::0x%x",
                      Z2S_DEVICES_LIST[i].manufacturer_name, Z2S_DEVICES_LIST[i].model_name, i, 
                      Z2S_DEVICES_LIST[i].z2s_device_endpoints_count,
                      Z2S_DEVICES_LIST[i].z2s_device_endpoints[0].endpoint_id, Z2S_DEVICES_LIST[i].z2s_device_endpoints[0].z2s_device_desc_id,
                      Z2S_DEVICES_LIST[i].z2s_device_endpoints[1].endpoint_id, Z2S_DEVICES_LIST[i].z2s_device_endpoints[1].z2s_device_desc_id,
                      Z2S_DEVICES_LIST[i].z2s_device_endpoints[2].endpoint_id, Z2S_DEVICES_LIST[i].z2s_device_endpoints[2].z2s_device_desc_id,
                      Z2S_DEVICES_LIST[i].z2s_device_endpoints[3].endpoint_id, Z2S_DEVICES_LIST[i].z2s_device_endpoints[3].z2s_device_desc_id );

              for (int j = 0; j < Z2S_DEVICES_LIST[i].z2s_device_endpoints_count; j++) {
              
                uint8_t endpoint_id = ( Z2S_DEVICES_LIST[i].z2s_device_endpoints_count == 1) ? 
                                        1 : Z2S_DEVICES_LIST[i].z2s_device_endpoints[j].endpoint_id; 
                                        
                uint32_t z2s_device_desc_id = ( Z2S_DEVICES_LIST[i].z2s_device_endpoints_count == 1) ?
                                                Z2S_DEVICES_LIST[i].z2s_device_desc_id :
                                                Z2S_DEVICES_LIST[i].z2s_device_endpoints[j].z2s_device_desc_id; 

                for (int k = 0; k < devices_desc_table_size; k++) {

                  if ( z2s_device_desc_id == Z2S_DEVICES_DESC[k].z2s_device_desc_id) {
                  log_i("DESC matched 0x%x, %d, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, endpoint 0x%x ",
                        Z2S_DEVICES_DESC[k].z2s_device_desc_id,   
                        Z2S_DEVICES_DESC[k].z2s_device_clusters_count,
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[0],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[1],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[2],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[3],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[4],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[5],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[6],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[7],
                        endpoint_id);

                        device_recognized = true;

                        joined_device->endpoint = endpoint_id;
                        joined_device->model_id = Z2S_DEVICES_DESC[k].z2s_device_desc_id;
                        
                        Z2S_addZ2SDevice(joined_device);
                        
                        for (int m = 0; m < Z2S_DEVICES_DESC[k].z2s_device_clusters_count; m++)
                          zbGateway.bindDeviceCluster(joined_device, Z2S_DEVICES_DESC[k].z2s_device_clusters[m]);
                  }  
                  else 
                  log_i("DESC checking 0x%x, %d, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, endpoint %d ",
                        Z2S_DEVICES_DESC[k].z2s_device_desc_id,   
                        Z2S_DEVICES_DESC[k].z2s_device_clusters_count,
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[0],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[1],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[2],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[3], 
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[4],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[5],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[6],
                        Z2S_DEVICES_DESC[k].z2s_device_clusters[7],
                        endpoint_id);        
                  }
              }  
            }   
            else log_i("LIST checking %s::%s, entry # %d",Z2S_DEVICES_LIST[i].manufacturer_name, Z2S_DEVICES_LIST[i].model_name, i);
          }
      if (!device_recognized) log_d("Unknown model %s::%s, no binding is possible", zbd_manuf_name, zbd_model_name);
    }
  }
}