#pragma once
// Test de presence / ouverture de session des pilotes NI hors DAQmx (sans en-tete, via QLibrary).
//   5108 -> NI-SCOPE (niScope_64.dll)   6570 -> niDigital (niDigital_64.dll)   6537 -> NI-HSDIO (niHSDIO_64.dll)
#include <QString>

namespace DriverProbe {
enum Kind { Scope, Digital, Hsdio };
QString dllName(Kind k);
// Charge la DLL puis tente d'ouvrir (et refermer aussitot) une session sur la ressource donnee.
// Ne change aucune sortie (pas de reset). Renvoie un texte multi-lignes lisible.
QString probe(Kind k, const QString& resource, bool openSession);
}
