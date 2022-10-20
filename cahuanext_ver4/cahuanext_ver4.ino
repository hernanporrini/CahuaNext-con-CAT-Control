  /* 
  CAHUANEXT VER4.0 LW1EHP (HERNAN ROBERTO PORRINI)
  se incorporo la funcionalidad de CAT (Icom IC746)
  --------------------------------------------------------------------
              OLAVARRIA 20/10/2022
      --------------------------------------------------------------------
  ESTE DISEÑO ESTA CONSTRUIDO EN BASE AL TRABAJO ORIGINAL DE: @REYNICO   
  LINK DEL PROYECTO: https://github.com/reynico/arduino-dds
  DESCONOZCO CUAL ES LA FUENTE DEL TRABAJO ANTERIOR.


  SOFTWARE DE CONTROL PARA UN DDS EN CAHUANE FR-300 DE 3 CANALES
  BANDAS DE OPERACION: 40-30-20 METROS
  MODOS: USB(bls) y LSB(bli)
  PASOS: 10KHz, 1KHz, 100Hz Y 10 Hz
  INDICACION DE RX-METER (señal AGC) 
  INDICACION DE TX-METER (señal MOD) 
  MODULO DDS: AD9850
  MICROCONTROLADOR: ATMEGA2560
  DISPLAY: LCD16X02 CONEXION PARALELO
        
  COMPENSACION DE FRECUENCIA, EXISTE UN ERROR ENTRE LA FRECUENCIA DESEADA(DISPLAY) Y LA GENERADA.
  SUPER IMPORTANTE BUSCAR LA COMPENSACION EN LA LINEA 254 PARA LSB Y LA LINEA 258 PARA USB.

  */


// https://github.com/arduino-libraries/LiquidCrystal
#include "lib/LiquidCrystal.h"
#include "lib/LiquidCrystal.cpp"

// https://github.com/thijse/Arduino-EEPROMEx
#include "lib/EEPROMex.h"
#include "lib/EEPROMex.cpp"

// https://github.com/buxtronix/arduino/tree/master/libraries/Rotary
#include "lib/Rotary.h"
#include "lib/Rotary.cpp"

// https://github.com/marcobrianza/ClickButton
#include "lib/ClickButton.h"
#include "lib/ClickButton.cpp"

// https://github.com/kk4das/IC746CAT
#include "lib/IC746.h"
#include "lib/IC746.cpp"

IC746 radio = IC746();

// radio modes
#define MODE_LSB 00
#define MODE_USB 01
byte Mode = MODE_USB;

#define PTT_RX 0
#define PTT_TX 1
byte ptt = PTT_RX;

// Pantalla LCD
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

// Libreria ClickButton
int step_button_status = 0;
ClickButton step_button(53, LOW, CLICKBTN_PULLUP);

#include "common.h"
#include "ad9850.h"
#include "smeter.h"
#include "rotary.h"

// function to run when we must put radio on TX/RX
void catSetPtt(boolean catPTT) {
  // the var ptt follows the value passed, but you can do a few more thing here
  if (catPTT) 
    {ptt = PTT_TX;} 
  else 
    {ptt = PTT_RX;}
}

boolean catGetPtt() {
  if (ptt == PTT_TX) {
    return true;
  } else {
    return false;
  }
}

// function to set a freq from CAT
void catSetFreq(long f) 
  {
  rx = f;
  }

// function to set the mode from the cat command
void catSetMode(byte m) 
  {
  if (m == CAT_MODE_LSB) 
    {
    mode = "LSB";
    Mode = MODE_LSB;
    } 
  else 
    {
    mode = "USB";
    Mode = MODE_USB;
    }
  }

// function to pass the freq to the cat library
long catGetFreq() 
  {
  long f;
  f = rx;
  return f;
  }

// function to pass the mode to the cat library
byte catGetMode() 
  {
  byte catMode;
  if (mode == "LSB") 
    {
    catMode = CAT_MODE_LSB;
    } 
  else 
    {
    catMode = CAT_MODE_USB;
    }
  return catMode;
  }

void setup() {
  lcd.begin(16, 2);
  
  radio.addCATPtt(catSetPtt);
  radio.addCATFSet(catSetFreq);
  radio.addCATMSet(catSetMode);
  
  radio.addCATGetPtt(catGetPtt);
  radio.addCATGetFreq(catGetFreq);
  radio.addCATGetMode(catGetMode);
  
  // now we activate the library
  radio.begin(19200, SERIAL_8N1);

  /* 
     Creamos los caracteres especiales
     para mostrar en el S-meter.
  */
  byte fillbar[8] = {
    B00000,
    B01000,
    B01100,
    B01010,
    B01001,
    B01010,
    B01100,
    B01000
  };
  byte mark[8] = {
    B00000,
    B01010,
    B10001,
    B10101,
    B10001,
    B01010,
    B00000,
    B00000
  };
  lcd.createChar(0, fillbar);
  lcd.createChar(1, mark);

  /*
     Inicializamos un timer para el encoder rotativo
     de esta manera el encoder no bloquea el flujo
     normal de loop();
  */
  PCICR |= (1 << PCIE0);
  PCMSK0 |= (1 << PCINT6) | (1 << PCINT7);
  sei();

  /*
     Configuramos 4 salidas para controlar el AD9850.
  */
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT);
  pulse_high(RESET);
  pulse_high(W_CLK);
  pulse_high(FQ_UD);

  /*
     Configuramos tres salidas para
     el cambio de banda por relay.
  */
  pinMode(30, INPUT_PULLUPS); // entrada de MICPTT
  pinMode(40, OUTPUT); // RELAY PTT
  pinMode(41, OUTPUT); // RELAY banda 40m
  pinMode(42, OUTPUT); // RELAY banda 30m
  pinMode(43, OUTPUT); // RELAY banda 20m

  /*
     El código contempla el uso de la memoria EEPROM del
     Arduino. De esta manera podemos almacenar la última
     frecuencia utilizada y recuperarla al encender el
     equipo.
  */
  if (EEPROM.read(0) == true) {
    Sprintln("Leyendo memoria EEPROM...");
    initial_band = EEPROM.read(1);
    initial_rx = EEPROM.readLong(2);
  } else {
    Sprintln("La memoria EEPROM está vacía. Inicializando con valores por defecto...");
  }
  for (int j = 0; j < 8; j++)
    lcd.createChar(j, block[j]);
  lcd.setCursor(0, 0);
  lcd.print("-CAHUA_NEXT--cat");
  lcd.setCursor(0, 1);
  lcd.print("Ver.:4.00 LW1EHP");
  delay (5000);                // SALUDO INICIAL POR 5 SEGUNDOS
  lcd.setCursor(0, 0);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  if_freq = 2000000;           // FRECUENCIA DEL FILTRO fi
  add_if = false;              
  Mode = MODE_LSB;
  mode= "LSB";
    //
  //mode = vfo_mode(initial_band);
  lcd.setCursor(0, 0);
  lcd.print(mode);
  increment=1000;
}

void loop() 
  {
  /* El cambio de banda se realiza con una llave
      giratoria que pone a masa una de las tres
      entradas por vez.
  */
  radio.check();
  
  if (6500000<rx<7500000 && band != 40) {
    digitalWrite(41, HIGH);
    digitalWrite(42, LOW);
    digitalWrite(43, LOW);
    band = 40;
    add_if = false;
    lcd.setCursor(0, 0);
    lcd.print("LSB");
    Mode = MODE_LSB;
    mode= "LSB";
    //mode = vfo_mode(band);
    rx =7100000;
    }
  
  if (9500000<rx<10500000 && band != 30) 
    {
    digitalWrite(41, LOW);
    digitalWrite(42, HIGH);
    digitalWrite(43, LOW);
    band = 30;
    add_if = false;
    lcd.setCursor(0, 0);
    lcd.print("LSB");
    mode= "LSB";
    Mode = MODE_LSB;
    //mode = vfo_mode(band);
    rx =10100000;
    }
  
  if (13500000<rx<14500000 && band != 20) 
    {
    digitalWrite(41, LOW);
    digitalWrite(42, LOW);
    digitalWrite(43, HIGH);
    band = 20;
    add_if = false;
    lcd.setCursor(0, 0);
    lcd.print("LSB");
    mode= "LSB";
    Mode = MODE_LSB;
    //mode = vfo_mode(band);
    rx =14100000;
    }

  /*
     Si la frecuencia mostrada es diferente a la frecuencia que
     obtengo al tocar el encoder, la actualizo tanto en la
     pantalla como en el DDS AD9850.
  */
  if (rx != rx2) 
    {
    show_freq();
    send_frequency(rx);
    rx2 = rx;
    }

  /*
     step_button es el botón del encoder rotativo. Con un click
     corto cicla entre los diferentes saltos de incremento. Con
     dos clicks cortos cicla entre los diferentes saltos de
     decremento. Con un click largo cambia entre los modos USB y LSB  
  */
  step_button.Update();
  if (step_button.clicks != 0) 
    {
    step_button_status = step_button.clicks;
    }
  if (step_button.clicks == 1) 
    {
    set_increment();
    }
  if (step_button.clicks == 2) 
    {
    set_decrement();
    }
  if (step_button.clicks == -1) 
    {
    if (mode == "USB") 
      { 
      Mode = MODE_LSB;
      mode = "LSB";
      if_freq = 2000000;
      add_if = false;
      }
    else if (mode == "LSB") 
      {
      Mode = MODE_USB;
      mode = "USB";
      if_freq = 2000000;
      add_if = true;
      }
    lcd.setCursor(0, 0);
    lcd.print(mode);
    }

  /*
     Luego de sintonizar una frecuencia, si pasaron mas de dos
     segundos la misma se almacena en la memoria EEPROM.
  */
  if (!mem_saved) 
    {
    if (time_passed + 2000 < millis()) 
      {
      store_memory();
      }
    }

  /*
     send_Frequency envía la señal al AD9850.
  */
  //send_frequency(rx);
  if(mode == "LSB")
    {
    send_frequency(rx+180);                     //COMPENSACION DE FRECUENCIA, EXISTE UN ERROR ENTRE LA FRECUENCIA DESEADA(DISPLAY) Y LA GENERADA
    }
    else   if(mode == "USB")
    {
    send_frequency(rx+320);
    }
 
  if (millis() < lastT)
    return;
  lastT += T_REFRESH;
   /*
     El S-meter funciona tanto para transmisión (MOD) como para recepción (AGC). 
     La señal del AGC se lee por (A1).
     La señal del MOD se lee por (A2).
     
  */
  
  ptt=!digitalRead(30); //se lee el estado del PTTMIC y se guarda en la variable ptt
  
  if (ptt) {
    digitalWrite(40, HIGH);
    lcd.setCursor(4, 0);
    lcd.print("TX");
    smeter_signal = map(sqrt(analogRead(A2) * 16), 0, 128, 0, 80);
    smeter(1, smeter_signal);
    } 
    else 
    {
    digitalWrite(40, LOW);
    lcd.setCursor(4, 0);
    lcd.print("RX");
    smeter_signal = map(sqrt(analogRead(A1) * 16), 40, 128, 0, 100);
    smeter(1, smeter_signal);
    }
  
}

/*
   El incremento de frecuencia se hace de manera
   cíclica por los valores definidos abajo. Cada
   vez que se llama a la función set_increment()
   ésta prepara automáticamente el próximo valor.
*/
void set_increment() 
  {
  switch (increment) 
    {
    case 1:
      increment = 10;
      hertz = "  10 Hz";
      hertz_position = 11;
      break;
    case 10:
      increment = 100;
      hertz = " 100 Hz";
      hertz_position = 10;
      break;
    case 100:
      increment = 1000;
      hertz = "  1 kHz";
      hertz_position = 11;
      break;
    case 1000:
      increment = 10000;
      hertz = " 10 kHz";
      hertz_position = 10;
      break;
    }
  lcd.setCursor(3, 0);
  lcd.print("             ");
  lcd.setCursor(6, 0);
  lcd.print(hertz);
  delay(1000);
  show_freq();
  }

void set_decrement() 
  {
  switch (increment) 
    {
    case 100000:
      increment = 10000;
      hertz = " 10 kHz";
      hertz_position = 11;
      break;
    case 10000:
      increment = 1000;
      hertz = "  1 kHz";
      hertz_position = 10;
      break;
    case 1000:
      increment = 100;
      hertz = " 100 Hz";
      hertz_position = 9;
      break;
    case 100:
      increment = 10;
      hertz = "  10 Hz";
      hertz_position = 11;
      break;
    case 10:
      increment = 1;
      hertz = "   1 Hz";
      hertz_position = 11;
      break;
    }
  lcd.setCursor(3, 0);
  lcd.print("             ");
  lcd.setCursor(6, 0);
  lcd.print(hertz);
  delay(1000);
  show_freq();
  }

/*
   Ésta función se encarga de mostrar la frecuencia
   en pantalla.
*/
void show_freq() 
  {
  lcd.setCursor(3, 0);
  lcd.print("             ");
  if (rx < 10000000) 
    {
    lcd.setCursor(7, 0);
    } 
    else 
    {
    lcd.setCursor(6, 0);
    }
  lcd.print(rx / 1000000);
  lcd.print(".");
  lcd.print((rx / 100000) % 10);
  lcd.print((rx / 10000) % 10);
  lcd.print((rx / 1000) % 10);
  lcd.print(".");
  lcd.print((rx / 100) % 10);
  lcd.print((rx / 10) % 10);
  lcd.print((rx / 1) % 10);
  time_passed = millis();
  mem_saved = false; // Triggerea el guardado en memoria
  }

/*
   store_memory() guarda la frecuencia elegida en la memoria
   EEPROM cada vez que se llama.
*/
void store_memory() 
  {
  EEPROM.write(0, true);
  EEPROM.write(1, band);
  EEPROM.writeLong(2, rx);
  mem_saved = true;
  }

