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
const float pressionArretPompe = 0.43;   // Pression seuil d'arrêt (0.45 bar - 5%)

// MESURES RÉELLES DE VOTRE CAPTEUR
// Cuve vide : 4.28mA = 0.00 bar
// Cuve pleine statique : 7.46mA = 0.21 bar
// Cuve pleine dynamique (pompe en marche) : 0.45 bar
const float currentVide = 4.28;     // 4.28mA mesuré
const float currentPleine = 7.46;   // 7.46mA mesuré
const float pressionVide = 0.00;    // Pression à cuve vide
const float pressionPleine = 0.21;  // Pression à cuve pleine statique
const float pressionPleineDynamique = 0.45;  // Pression réelle en dynamique (pompe active)

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

// État de la pompe (avec hystérésis pour éviter les cycles rapides)
bool pompeActive = false;

void setup() {
  Serial.begin(9600);
  
  // Initialiser toutes les LEDs en sortie
  for (int i = 0; i < nbLeds; i++) {
    pinMode(leds[i], OUTPUT);
    digitalWrite(leds[i], LOW);
  }
  
  // Initialiser le pin du SSR
  pinMode(pinSSR, OUTPUT);
  digitalWrite(pinSSR, LOW);  // Pompe arrêtée au démarrage
  
  // Initialiser le tableau des mesures
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }
  
  // Test des LEDs au démarrage
  testLeds();
  
  Serial.println("========================================");
  Serial.println("CAPTEUR NIVEAU CUVE");
  Serial.println("Plage: 0.00 bar (vide) à 0.21 bar (pleine)");
  Serial.println("Dynamique pompe: 0.45 bar max");
  Serial.println("Courant: 4.28mA à 7.46mA");
  Serial.println("Pompe: Démarre à 15% / Arrête à 0.43 bar");
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
  float current = (voltage / R_shunt) * 1000;
  
  // Conversion en pression selon vos mesures réelles
  float pression = convertirPressionReelle(current);
  
  // Mettre à jour l'affichage LED si changement significatif
  if (abs(pression - dernierePression) > 0.01) {
    dernierePression = pression;
  }
  
  // Mettre à jour les LEDs (géré dans la fonction pour le clignotement)
  afficherNiveauLEDs(pression);
  
  // Gestion de la pompe avec hystérésis
  gererPompe(pression);
  
  // Affichage série toutes les 5 secondes
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= affichageInterval) {
    previousMillis = currentMillis;
    
    // Calcul du pourcentage de remplissage
    float pourcentage = mapFloat(pression, pressionVide, pressionPleine, 0.0, 100.0);
    pourcentage = constrain(pourcentage, 0.0, 100.0);
    
    // Nombre de LEDs allumées
    int ledsAllumees = mapFloat(pression, pressionVide, pressionPleine, 0, nbLeds);
    ledsAllumees = constrain(ledsAllumees, 0, nbLeds);
    
    // Affichage formaté
    Serial.print(pression, 3);
    Serial.print(" bar | ");
    Serial.print(pourcentage, 1);
    Serial.print("% | ");
    
    // Barre graphique
    for (int i = 0; i < nbLeds; i++) {
      Serial.print(i < ledsAllumees ? "█" : "░");
    }
    
    // État texte
    Serial.print(" | ");
    if (pourcentage <= 5.0) {
      Serial.print("VIDE");
    } else if (pourcentage >= 95.0) {
      Serial.print("PLEINE");
    } else {
      Serial.print("Niveau ");
      Serial.print(ledsAllumees);
    }
    
    // État de la pompe
    Serial.print(" | ");
    Serial.println(pompeActive ? "ON" : "OFF");
  }
  
  delay(50);
}

// Conversion basée sur vos mesures réelles
float convertirPressionReelle(float courant) {
  if (courant <= currentVide) {
    return pressionVide;
  } else if (courant >= currentPleine) {
    return pressionPleine;
  } else {
    return pressionVide + (courant - currentVide) * (pressionPleine - pressionVide) / (currentPleine - currentVide);
  }
}

// Gestion de la pompe avec hystérésis
void gererPompe(float pression) {
  float pourcentage = mapFloat(pression, pressionVide, pressionPleine, 0.0, 100.0);
  pourcentage = constrain(pourcentage, 0.0, 100.0);
  
  // Hystérésis : démarre à 15%, s'arrête à 0.43 bar (mesure dynamique)
  if (!pompeActive && pourcentage <= seuilDemarragePompe) {
    // Niveau bas : démarrer la pompe
    pompeActive = true;
    digitalWrite(pinSSR, HIGH);
    Serial.println(">>> POMPE DÉMARRÉE (niveau bas) <<<");
  } 
  else if (pompeActive && pression >= pressionArretPompe) {
    // Pression dynamique atteinte : arrêter la pompe
    pompeActive = false;
    digitalWrite(pinSSR, LOW);
    Serial.print(">>> POMPE ARRÊTÉE (");
    Serial.print(pression, 3);
    Serial.println(" bar atteints) <<<");
  }
}

// Affichage sur les LEDs avec gestion du clignotement
void afficherNiveauLEDs(float pression) {
  float pourcentage = mapFloat(pression, pressionVide, pressionPleine, 0.0, 100.0);
  pourcentage = constrain(pourcentage, 0.0, 100.0);
  
  int ledsAllumees = mapFloat(pression, pressionVide, pressionPleine, 0, nbLeds);
  ledsAllumees = constrain(ledsAllumees, 0, nbLeds);
  
  // Gestion du clignotement pour niveau critique (≤10%)
  static unsigned long dernierClignotement = 0;
  static bool etatClignotement = false;
  
  if (pourcentage <= 10.0) {
    // Clignotement toutes les secondes
    if (millis() - dernierClignotement >= 1000) {
      dernierClignotement = millis();
      etatClignotement = !etatClignotement;
    }
    
    // Mode clignotement : première LED clignote, autres éteintes
    digitalWrite(leds[0], etatClignotement ? HIGH : LOW);
    for (int i = 1; i < nbLeds; i++) {
      digitalWrite(leds[i], LOW);
    }
  } else {
    // Affichage normal : barre progressive
    for (int i = 0; i < nbLeds; i++) {
      digitalWrite(leds[i], i < ledsAllumees ? HIGH : LOW);
    }
  }
}

// Test des LEDs au démarrage
void testLeds() {
  // Remplissage progressif
  for (int i = 0; i < nbLeds; i++) {
    digitalWrite(leds[i], HIGH);
    delay(150);
  }
  delay(500);
  
  // Vidage progressif
  for (int i = nbLeds - 1; i >= 0; i--) {
    digitalWrite(leds[i], LOW);
    delay(150);
  }
  delay(500);
  
  // Simulation des niveaux de 0 à 0.21 bar
  for (float p = pressionVide; p <= pressionPleine; p += 0.03) {
    int ledsAllumees = mapFloat(p, pressionVide, pressionPleine, 0, nbLeds);
    ledsAllumees = constrain(ledsAllumees, 0, nbLeds);
    for (int i = 0; i < nbLeds; i++) {
      digitalWrite(leds[i], i < ledsAllumees ? HIGH : LOW);
    }
    delay(200);
  }
  
  // Éteindre toutes les LEDs à la fin du test
  for (int i = 0; i < nbLeds; i++) {
    digitalWrite(leds[i], LOW);
  }
  delay(500);
}

// Fonction map pour les floats
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}