#include "DriverProbe.h"
#include <QLibrary>
#include <QByteArray>
#include <cstdint>

namespace {
typedef int32_t  ViStatus;
typedef uint32_t ViSession;
typedef uint16_t ViBoolean;

typedef ViStatus (*InitFn)(const char*, ViBoolean, ViBoolean, ViSession*);
typedef ViStatus (*HsdioInitFn)(const char*, ViBoolean, ViBoolean, const char*, ViSession*);
typedef ViStatus (*CloseFn)(ViSession);
typedef ViStatus (*ErrMsgFn)(ViSession, ViStatus, char*);
}

namespace DriverProbe {

QString dllName(Kind k)
{
    switch (k) {
    case Scope:   return "niScope_64";
    case Digital: return "niDigital_64";
    case Hsdio:   return "niHSDIO_64";
    }
    return {};
}

QString probe(Kind k, const QString& resource, bool openSession)
{
    QString out;
    QLibrary lib(dllName(k));
    if (!lib.load()) {
        return QString("DLL %1 introuvable : pilote non installe (ou pas dans le PATH).\n%2")
                   .arg(dllName(k), lib.errorString());
    }
    out += QString("DLL %1 chargee : OK\n").arg(dllName(k));
    if (!openSession) return out;

    const char* initName  = k == Scope ? "niScope_init" : (k == Digital ? "niDigital_init" : "niHSDIO_InitGenerationSession");
    const char* closeName = k == Scope ? "niScope_close" : (k == Digital ? "niDigital_close" : "niHSDIO_close");
    const char* msgName   = k == Digital ? "niDigital_error_message" : (k == Hsdio ? "niHSDIO_error_message" : nullptr);

    void* initP  = (void*)lib.resolve(initName);
    void* closeP = (void*)lib.resolve(closeName);
    if (!initP || !closeP) {
        return out + QString("Fonction %1 / %2 introuvable dans la DLL (version de pilote inattendue).")
                         .arg(initName, closeName);
    }
    const QByteArray rsrc = resource.toLocal8Bit();
    ViSession vi = 0;
    ViStatus st;
    if (k == Hsdio) st = ((HsdioInitFn)initP)(rsrc.constData(), 0, 0, "", &vi);
    else            st = ((InitFn)initP)(rsrc.constData(), 0, 0, &vi);

    if (st < 0) {
        out += QString("Ouverture de session sur \"%1\" : ECHEC, code %2\n").arg(resource).arg(st);
        if (msgName) {
            ErrMsgFn m = (ErrMsgFn)lib.resolve(msgName);
            if (m) { char buf[512] = {0}; m(vi, st, buf); out += QString("Message du pilote : %1\n").arg(QString::fromLocal8Bit(buf)); }
        }
        out += "Verifie le nom exact de la ressource dans NI MAX (ex. PXI1Slot5).";
        return out;
    }
    if (st > 0) out += QString("(avertissement pilote : code %1)\n").arg(st);
    out += QString("Ouverture de session sur \"%1\" : OK (session %2)\n").arg(resource).arg(vi);
    ((CloseFn)closeP)(vi);
    out += "Session refermee proprement.";
    return out;
}
}
