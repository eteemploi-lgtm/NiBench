# NiBench - banc d'essai NI (test de communication)

Petite application Qt pour verifier, carte par carte, que le PC parle bien au chassis, AVANT d'utiliser l'IHM Acoustimeter.

## Compilation (PowerShell)

```powershell
$env:PATH = "D:\Qt2\Tools\CMake_64\bin;D:\Qt2\Tools\Ninja;" + `
            "D:\Qt2\Tools\mingw1310_64\bin;D:\Qt2\6.11.1\mingw_64\bin;" + $env:PATH
cd D:\projet\NiBench
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="D:/Qt2/6.11.1/mingw_64"
cmake --build build
.\build\NiBench.exe
```

(NI-DAQmx est active par defaut. Pour compiler sans NI : `-DNIBENCH_WITH_NI=OFF`.)

## Utilisation

1. **SE CONNECTER** : liste les cartes vues par NI-DAQmx (nom, modele, serie, nombre de voies).
2. Onglet **Ecoute** : choisir carte/voie, puis DEMARRER. Presets 4353 (thermocouple) et 4480 (tension, FFT).
3. Onglet **Numerique** : LED des entrees (lecture seule) ; sorties L0..L7 + clignotement (case « Autoriser les SORTIES » obligatoire).
4. Onglet **Sortie analogique** : tension DC, seulement si une carte a des sorties analogiques.
5. Onglet **Pilotes** : teste que les DLL NI-SCOPE (5108), niDigital (6570) et NI-HSDIO (6537) sont installees et qu'une session s'ouvre.
6. **Copier le rapport complet** (ou Enregistrer) et colle-le dans la conversation.

## Securite
Laisse la sortie de l'amplificateur DEBRANCHEE du transducteur. Les sorties sont verrouillees tant que la case rouge en haut n'est pas cochee.
