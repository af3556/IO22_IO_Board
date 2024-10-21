
unsigned long sec = 0;
int i = -5;
int currentStep = 0;


void printInputsStatus()
{
  // Serial.println();
  for (byte b = 0; b < INPUTS; b++)
    if (inValues[b] > 1) {
      Serial.print("IN"); Serial.print(b + 1); Serial.print(": ");
      if (inValues[b] == 2)  Serial.println("RISING"); else Serial.println("FALLING");
    }

  for (byte b = 0; b < KEYS; b++)
    if (keysValues[b] > 1) {
      Serial.print(" K"); Serial.print(b + 1); Serial.print(": ");
      if (keysValues[b] == 2)  Serial.println("RISING"); else Serial.println("FALLING");

    }


}


void DEMO()
{
  //DEMO
  if (millis() - sec > 500)
  {
    if (USE_LCD) {

      // int test
      if (currentStep == 0)
      {
        if (i == -5) Serial.println("print int numbers");
        setLCDbyInt(i);
        i++;
        if (i == 1) {
          currentStep = 1;
          i = 0;
        }
      }

      // chars test
      if (currentStep == 1)
      {
        if (i == 0)
        {
          Serial.println("print all chars");
          setLCDbyInt(0);
        }
        else
          setLCDdigit(3, i);  // zero based 3- its 4th segment
        i++;
        if (i == 41) {
          currentStep = 2;
          i = -1;
        }

      }

      // time test
      if (currentStep == 2)
      {

        if (i == -1) Serial.println("time test");
        else
        { if (i % 2 == 0)
            setLCDTime(12, i / 2, i % 3 > 0);
          else  setLCDTime(12, i / 2, i % 3 > 0);
        }

        i++;
        if (i == 10) {
          currentStep = 3;
          i = -1;
        }
      }

      // print text
      if (currentStep == 3)
      {
        if (i == -1) Serial.println("print text");
        else
        { if (i < 3) setLCDtext("STAR"); else setLCDtext("END");
        }
        i++;
        if (i == 6) {
          currentStep = 4;
          i = -1;
        }
      }

      // custom chars

      /*---Segment Display Screen----
        --A--
        F---B
        --G--
        E---C
        --D--
         __  __   __  __
        |__||__|.|__||__|
        |__||__|'|__||__|
        ----------------------*/
      if (currentStep == 4)
      {
        if (i == -1) Serial.println("custom chars");
        else
        { //          seg,a,b,c,d,e,f,g    // seg zero based 0- its 1th segment
          if (i == 0) {
            clearLCD();
            setCustomChar(0, 1, 0, 0, 1, 0, 0, 1);
          }
          if (i == 1) setCustomChar(1, 0, 1, 0, 0, 0, 1, 0);
          if (i == 2) setCustomChar(2, 0, 0, 1, 0, 1, 0, 0);
          if (i == 3) setCustomChar(3, 1, 1, 0, 1, 1, 0, 0);
        }
        i++;
        if (i > 4) {
          currentStep = 5;
          i = -1;
        }
      }
    }

    // relays test
    if (currentStep == 5 || !USE_LCD)
    {
      if (i < 1)
      {
        Serial.println("relays test");
        if (USE_LCD)
          setLCDtext("REL ");
      }
      else
      {
        if (i > 0 && i < 9) setRelay(i);
        if (i > 8 && i < 17) resetRelay(i - 8);
      }
      i++;
      if (i > 17) {
        currentStep = 0;
        clearRelays();
        if (USE_LCD) i = -5; else i = -1;
      }
    }

    sec = millis();
  }


  printInputsStatus(); // print status only on IO_RISED and IO_FALLING

}
