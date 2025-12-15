#include <Arduino.h>

const int analogPin = A0;
const float R_shunt = 217.0;
const int numReadings = 10;

// Pins des LEDs (D3 à D10)
const int leds[] = {3, 4, 5, 6, 7, 8, 9, 10};
const int nbLeds = 8;

// Pin pour le SSR de la pompe
const int pinSSR = 11;

// Seuils de démarrage et arrêt de la pompe
const float seuilDemarragePompe = 15.0;  // Démarre à 15% de remplissage
const float seuilArretPompe = 95.0;      // S'arrête à 95% (plein -5%)
const float pressionArretPompe = 0.45;   // Pression seuil d'arrêt (0.45 bar)

// === PLAGE COMPLÈTE pour affichage pression : 0-1 BAR ===
const float courantVide = 4.28;      // 4.28mA = 0.00 bar
const float courantPleine = 20.0;    // 20.0mA = 1.00 bar (standard)

const float pressionMin = 0.00;      // 0% = 0.00 bar
const float pressionMax = 1.00;      // 100% = 1.00 bar

// === PLAGE LIMITÉE pour affichage LED : 0-0.21 BAR ===
const float pressionMaxLED = 0.21;   // 0.21 bar = toutes les LEDs allumées

// Variables pour la moyenne glissante
int readings[numReadings];
int readIndex = 0;
int total = 0;
int average = 0;

// Pour l'affichage série périodique
unsigned long previousMillis = 0;
const unsigned long affichageInterval = 5000;

// Dernière pression pour éviter les mises à jour inutiles des LEDs
float dernierePression = -1.0;

// État de la pompe
bool pompeActive = false;
bool initialisationTerminee = false;

void setup() {
  Serial.begin(9600);
  
  // Initialiser toutes les LEDs en sortie
  for (int i = 0; i < nbLeds; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }
  
  // Initialiser le pin du SSR
  pinMode(pinSSR, OUTPUT);
  digitalWrite(pinSSR, LOW);
  
  // Initialiser le tableau des mesures
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }
  
  // Test des LEDs au démarrage
  testLeds();
  
  Serial.println("========================================");
  Serial.println("CAPTEUR NIVEAU CUVE");
  Serial.println("Affichage pression: 0.00 - 1.00 bar");
  Serial.println("Affichage LED: 0.00 - 0.21 bar (0.21 = toutes LEDs)");
  Serial.println("Courant: 4.28mA (0 bar) à 20.0mA (1 bar)");
  Serial.println("Pompe: Démarre à 0.15 bar / Arrête à 0.45 bar");
  Serial.println("========================================");
  Serial.println("Pression | % Remplissage | Barre LEDs | Pompe");
  Serial.println("---------|---------------|------------|------");
}

void loop() {
  // Mise à jour de la moyenne glissante
  total = total - readings[readIndex];
  readings[readIndex] = analogRead(analogPin);
  total = total + readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  average = total / numReadings;
  
  // Calcul de la pression
  float voltage = average * (5.0 / 1023.0);
  float courant = (voltage / R_shunt) * 1000;
  
  // === CONVERSION POUR AFFICHAGE : 0-1 bar ===
  float pression = convertir4_20mA_vers_Bar(courant);
  
  // Phase d'initialisation
  static int compteurInit = 0;
  if (!initialisationTerminee) {
    compteurInit++;
    if (compteurInit >= numReadings * 2) {
      initialisationTerminee = true;
      Serial.print("Initialisation terminée - Pression: ");
      Serial.print(pression, 3);
      Serial.println(" bar");
      
      float pourcentageLED = mapFloat(pression, 0.0, pressionMaxLED, 0.0, 100.0);
      if (pourcentageLED <= seuilDemarragePompe) {
        pompeActive = true;
        digitalWrite(pinSSR, HIGH);
        Serial.println(">>> État initial: POMPE ON (niveau bas) <<<");
      } else {
        pompeActive = false;
        digitalWrite(pinSSR, LOW);
        Serial.println(">>> État initial: POMPE OFF (niveau suffisant) <<<");
      }
    }
  }
  
  // Mettre à jour l'affichage LED si changement significatif
  if (abs(pression - dernierePression) > 0.01) {
    dernierePression = pression;
  }
  
  // Mettre à jour les LEDs (avec plage limitée 0-0.21 bar)
  afficherNiveauLEDs(pression);
  
  // Gestion de la pompe (utilise la pression réelle 0-1 bar)
  if (initialisationTerminee) {
    gererPompe(pression);
  }
  
  // Affichage série toutes les 5 secondes
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= affichageInterval) {
    previousMillis = currentMillis;
    
    // Pourcentage basé sur 0-1 bar
    float pourcentageReel = pression * 100.0;
    
    // Pourcentage pour l'affichage LED (0-0.21 bar)
    float pourcentageLED = mapFloat(pression, 0.0, pressionMaxLED, 0.0, 100.0);
    pourcentageLED = constrain(pourcentageLED, 0.0, 100.0);
    
    // Nombre de LEDs allumées (basé sur 0-0.21 bar)
    int ledsAllumees = mapFloat(pression, 0.0, pressionMaxLED, 0, nbLeds);
    ledsAllumees = constrain(ledsAllumees, 0, nbLeds);
    
    // Affichage formaté
    Serial.print(pression, 3);
    Serial.print(" bar | ");
    Serial.print(pourcentageReel, 1);  // Pourcentage réel 0-100%
    Serial.print("% | ");
    
    // Barre graphique (basée sur 0-0.21 bar)
    for (int i = 0; i < nbLeds; i++) {
      Serial.print(i < ledsAllumees ? "█" : "░");
    }
    
    // État texte (basé sur affichage LED)
    Serial.print(" | ");
    if (pourcentageLED <= 5.0) {
      Serial.print("VIDE");
    } else if (pression >= pressionMaxLED) {
      Serial.print("MAX LED");
    } else if (pourcentageLED >= 95.0) {
      Serial.print("PLEINE");
    } else {
      Serial.print("Niveau ");
      Serial.print(ledsAllumees);
    }
    
    // État de la pompe
    Serial.print(" | ");
    Serial.print(pompeActive ? "ON" : "OFF");
    
    // Info supplémentaire
    Serial.print(" | Courant: ");
    Serial.print(courant, 2);
    Serial.print("mA | LED%: ");
    Serial.print(pourcentageLED, 1);
    Serial.println("%");
  }
  
  delay(50);
}

// Conversion 4-20mA → 0-1 bar
float convertir4_20mA_vers_Bar(float courant) {
  courant = constrain(courant, courantVide, courantPleine);
  return (courant - courantVide) * (pressionMax - pressionMin) / 
         (courantPleine - courantVide);
}

// Gestion de la pompe (utilise pression 0-1 bar)
void gererPompe(float pression) {
  // Pour la pompe, on utilise la pression réelle (0-1 bar)
  // ou on peut convertir en pourcentage LED si préféré
  float pourcentageLED = mapFloat(pression, 0.0, pressionMaxLED, 0.0, 100.0);
  
  if (!pompeActive && pourcentageLED <= seuilDemarragePompe) {
    pompeActive = true;
    digitalWrite(pinSSR, HIGH);
    Serial.println(">>> POMPE DÉMARRÉE (niveau bas) <<<");
  } 
  else if (pompeActive && pression >= pressionArretPompe) {
    pompeActive = false;
    digitalWrite(pinSSR, LOW);
    Serial.print(">>> POMPE ARRÊTÉE (");
    Serial.print(pression, 3);
    Serial.println(" bar atteints) <<<");
  }
}

// Affichage sur les LEDs (PLAGE LIMITÉE 0-0.21 bar)
void afficherNiveauLEDs(float pression) {
  // Convertir pression 0-1 bar → plage LED 0-0.21 bar
  float pressionLED = min(pression, pressionMaxLED);
  float pourcentageLED = mapFloat(pressionLED, 0.0, pressionMaxLED, 0.0, 100.0);
  
  int ledsAllumees = mapFloat(pressionLED, 0.0, pressionMaxLED, 0, nbLeds);
  ledsAllumees = constrain(ledsAllumees, 0, nbLeds);
  
  // Si pression > 0.21 bar, on fait clignoter la dernière LED
  static unsigned long dernierClignotement = 0;
  static bool etatClignotement = false;
  
  if (pression > pressionMaxLED) {
    // Mode dépassement : dernière LED clignote
    if (millis() - dernierClignotement >= 500) {
      dernierClignotement = millis();
      etatClignotement = !etatClignotement;
    }
    
    // Toutes les LEDs allumées + dernière clignote
    for (int i = 0; i < nbLeds - 1; i++) {
      digitalWrite(leds[i], HIGH);
    }
    digitalWrite(leds[nbLeds - 1], etatClignotement ? HIGH : LOW);
  }
  else if (pourcentageLED <= 10.0) {
    // Clignotement niveau critique (≤10%)
    if (millis() - dernierClignotement >= 1000) {
      dernierClignotement = millis();
      etatClignotement = !etatClignotement;
    }
    
    digitalWrite(leds[0], etatClignotement ? HIGH : LOW);
    for (int i = 1; i < nbLeds; i++) {
      digitalWrite(leds[i], LOW);
    }
  } else {
    // Affichage normal
    for (int i = 0; i < nbLeds; i++) {
      digitalWrite(leds[i], i < ledsAllumees ? HIGH : LOW);
    }
  }
}

// Test des LEDs au démarrage
void testLeds() {
  Serial.println("Test LEDs: 0.00 → 0.21 bar");
  
  // Simulation de 0.00 à 0.21 bar
  for (float p = 0.00; p <= pressionMaxLED; p += 0.03) {
    int ledsAllumees = mapFloat(p, 0.0, pressionMaxLED, 0, nbLeds);
    ledsAllumees = constrain(ledsAllumees, 0, nbLeds);
    
    Serial.print("Pression: ");
    Serial.print(p, 2);
    Serial.print(" bar → LEDs: ");
    Serial.println(ledsAllumees);
    
    for (int i = 0; i < nbLeds; i++) {
      digitalWrite(leds[i], i < ledsAllumees ? HIGH : LOW);
    }
    delay(300);
  }
  
  // Test dépassement (clignotement dernière LED)
  Serial.println("Test dépassement (>0.21 bar)");
  for (int i = 0; i < 10; i++) {
    digitalWrite(leds[nbLeds - 1], HIGH);
    delay(300);
    digitalWrite(leds[nbLeds - 1], LOW);
    delay(300);
  }
  
  // Éteindre toutes les LEDs
  for (int i = 0; i < nbLeds; i++) {
    digitalWrite(leds[i], LOW);
  }
  delay(500);
}

// Fonction map pour les floats
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}