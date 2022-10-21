/*
 * Pines a usar para CLK y DT del encoder rotativo
 */
Rotary r = Rotary(ROTARY_BAJAR, ROTARY_SUBIR);

/*
 * Rutina del encoder
 */
ISR(PCINT0_vect) {
  unsigned char result = r.process();
  if (result) {
    if (result == DIR_CW) {
      rx = rx + increment;
      
  
    }
    else {
      rx = rx - increment;
      
  
    }
    if (rx >= 30000000) { // Limite superior del VFO: 30MHz
      rx = rx2;
    }
    if (rx <= 1000000) { // Limite inferior del VFO: 1MHz
      rx = rx2;
    }
  }
}
