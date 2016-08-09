/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ns3/core-module.h>
#include <ns3/packet.h>

#include <ns3/average.h>
#include <ns3/csma-module.h>
#include <ns3/ipv4-header.h>
#include <ns3/ethernet-header.h>


using namespace ns3;



// Dos clases en el mismo fichero, para ajustarse a la entrega

class ObservadorDevice
{
public:
    ObservadorDevice ();

    void    PaqueteEnviado    (Ptr<const Packet> paquete);
    void    Colision          (Ptr<const Packet> paquete);

    double  MediaIntentos     ();

    void    EchoRequest       (Ptr<const Packet> paquete);
    void    EchoResponse      (Ptr<const Packet> paquete);

    Time    MediaEco          ();

    void    PaquetePerdido    (Ptr<const Packet> paquete);

    double  PorcenPaqPerdidos ();

    void    SetIdDevice(uint32_t idDevice);

private:
    uint32_t           m_intentos;                      // Contador de intentos de envío de los paquetes
    Average<uint32_t>  m_acumIntentos;                  // Acumulador para cálculos estadísticos de los intentos

    Time               m_tRequest;                      // Registro temporal del instante de solicitud de eco
    Average<int64_t>   m_acumtEco;                      // Acumulador para cálculos estadísticos de tiempo de eco (us)

    uint32_t           m_descartados;                   // Contador de paquetes que se descartan (por MaxRetries)

    uint32_t           m_idDevice;                      // Identificador del nodo asociado al observador. Para logs
};







class Observador
{
public:
    Observador (NetDeviceContainer * csmaDevices);
                                                                 // Resultados para:
    double   MediaIntentos     (uint32_t device);                // Dispositivo device indicado
    double   MediaIntentos     (uint32_t desde, uint32_t hasta); // Rango de dispositivos (extremos incluidos)

    Time     MediaEco          (uint32_t device);                // Cliente device indicado
    Time     MediaEco          (uint32_t desde, uint32_t hasta); // Rango de dispositivos (extremos incluidos)

    double   PorcenPaqPerdidos (uint32_t device);                // Dispositivo device indicado
    double   PorcenPaqPerdidos (uint32_t desde, uint32_t hasta); // Rango de dispositivos (extremos incluidos)

    ~Observador ()
    {
        /* 
            Se crean objetos de la clase ObservadorDevice que no se van a "destruir" automáticamente. Hago uso del
          método destructor ~Observador(),  para que cuando se destruya automáticamente el obj. Observador, se destruirá
          manualmente el array de objetos ObservadorDevice.
        */

        delete [] m_observadorDevice;
    }

private:
    ObservadorDevice * m_observadorDevice;                       // Array de objetos ObservadorDevice, uno por nodo
};