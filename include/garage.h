 //#include <structs.h>
    extern "C" {
#include "user_interface.h"
}
class Garage {
friend void timerCallbackWrapper(void *);
public:
   Garage();
   void initGarage(Settings settings, int relayPin);
   bool feed(Sensordata sd);  
   int getDistance();
   bool isOpen();
   bool hasCar();
   Status getStatus();
   String getState();
   void close();
   void open();
   void stop();
   void checkTimers();
   
   int distanceDoorOpen = 20;
   int distanceDoorClosed = 150;
private:
    bool toggleRelay(int state);
    bool garageDoorCompletelyOpen();
    Sensordata data;
    float variation = 0.20;
    int variationAmount = 0;
    unsigned long minStableTime = 2000;

    int stableDistance = 0;
    int currentDistance = 0;
    long feedTime = 0;
    Settings settings;
    bool relayState = LOW;
    unsigned long relayTimer = 0;
    int relayPin;
    os_timer_t Timer1;         
    int Counter = 0;           
    bool TickOccured = false;
    void timerCallback();  
    bool opening = false;
    bool closing = false;
    bool wasOpening = false;
    bool wasClosing = false;
    bool stopped = false;
    //unsigned long openingTimer = 0;
    unsigned long closingTimer = 0;

};


void timerCallbackWrapper(void *garage)
{ 
    Garage * g = (Garage *) garage;
    g->timerCallback();
}

Garage::Garage() 
{

}
void Garage::initGarage(Settings settings, int relayPin){
    this->settings = settings;
    this->relayPin = relayPin;
    this->distanceDoorOpen = settings.doorDistanceOpen;
    this->distanceDoorClosed = settings.doorDistanceClosed;
    //os_timer_setfn(&Timer1, timerCallbackWrapper, &Counter);
}

void Garage::timerCallback()
 {

 }

bool Garage::feed(Sensordata sd){
    data = sd;
    if(data.distance<=4){
        return false;
    }
    if(feedTime==0) {
        feedTime = data.lastUpdate;
        currentDistance = data.distance;
    } else {
        //check if distance is withoin variation limits
        variationAmount = currentDistance*variation;
        if( (data.distance<=(data.distance+variationAmount)) && (data.distance>=(data.distance-variationAmount)) ) {
          
            currentDistance = (currentDistance + data.distance)/2;
            if(millis() - feedTime >= minStableTime){
                feedTime = 0;
                stableDistance = currentDistance;
                return true;
            }
        } else {
            
        }
    }
    return false;

}

int Garage::getDistance() {
    return stableDistance;
}

bool Garage::garageDoorCompletelyOpen()
{
    return stableDistance <= distanceDoorOpen;
}

bool Garage::isOpen(){
    bool returnValue;
    if(garageDoorCompletelyOpen()) {
        //Garage is open
       return true;
    }
    else
    {
        //Garage is closing/closed
        if(closing)
        {
            //Garage is closing
            returnValue = true;
        }
        else
        {
            //Garage is closed
            returnValue = false;
        }
        
    }
    if(wasOpening && stopped) return !returnValue;
    else return returnValue;
}

bool Garage::hasCar(){
    if(stableDistance < (distanceDoorClosed*(1-variation)) && stableDistance > (distanceDoorOpen*(1-variation+1))) {
        return true;
    }
    return false;
}

String Garage::getState()
{
    if(opening) return "opening";
    if(closing) return "closing";
    return isOpen() ? "open" : "closed";
}

Status Garage::getStatus(){
    Status ret;
    ret.open = isOpen();
    ret.state = getState();
    ret.car = hasCar();
    ret.distance = stableDistance;
    ret.motion = data.motion;
    ret.lastUpdate = millis();
    return ret;
}

void Garage::open() {
     
    if(!isOpen() && !opening && !closing)
    {
        stopped = false;
        opening = true;
        wasClosing = false;
        wasOpening = true;
        //openingTimer = millis() + (this->settings.doorTimeOpen * 1000);
       // os_timer_arm(&Timer1, 20000, true);
        toggleRelay(1);
    }
}

void Garage::close() {
    if(isOpen() && !opening && !closing)
    {
        stopped = false;
        closing = true;
        wasClosing = true;
        wasOpening = false;
        closingTimer = millis() + (this->settings.doorTimeClose * 1000);
        //os_timer_arm(&Timer1, 20000, true);
        toggleRelay(0);
    }
}

void Garage::stop() {
    if(opening || closing)
    {
        toggleRelay(1);
        opening = false;
        //openingTimer = 0;
        closing = false;
        closingTimer = 0;
        stopped = true;
    }
}

bool Garage::toggleRelay(int state){
  bool cState = false;
  if(settings.relayMode==0){
    switch (state) {
      case 0:
        cState = LOW;
        break;
      case 1:
        cState = HIGH;
        break;
      case 2:
        cState = !relayState;
        break;
    }
    relayState = cState;
    digitalWrite(relayPin, cState);
    return cState;
  } else {

    Serial.println("{relay: true, mode: "+String(this->settings.relayMode)+"}");
    digitalWrite(relayPin, HIGH);
    relayTimer = millis() + this->settings.relayMode;
    return true;
  }
}

void Garage::checkTimers() {
    unsigned long milliseconds = millis();
    if(garageDoorCompletelyOpen())
    {
        opening = false;
        //openingTimer = 0;
    }
    if(closingTimer > 0 && milliseconds > closingTimer)
    {
        closing = false;
        closingTimer = 0;
    }
    if(relayTimer>0 && milliseconds > relayTimer){
      Serial.println("{relay: false, mode: "+String(settings.relayMode)+"}");
      digitalWrite(relayPin, LOW);
      relayTimer = 0;
    }
}




