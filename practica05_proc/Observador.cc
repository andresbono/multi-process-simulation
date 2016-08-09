/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "Observador.h"


NS_LOG_COMPONENT_DEFINE ("Observador");







Observador::Observador (NetDeviceContainer * csmaDevices)
{
    NS_LOG_FUNCTION_NOARGS (); // Sin argumentos. Aunque el constructor tiene argumentos, no nos aporta nada
    /* 
        Constructor de Observador. Recibe como argumento un puntero al contenedor de dispositivos ya que se hará toda la
      gestión de adquisición de estadísticos desde esta clase, y es necesario diferenciar cada uno de los nodos de la
      topología.

        En el constructor del Observador se hacen las suscripciones a las trazas necesarias. También se obtiene el
      número de nodos de la topología, que se podría pasar como argumento, pero así se hace la llamada lo más simple
      posible.
    */

    uint32_t nCsma = csmaDevices->GetN();             // Número de nodos en la topología

    m_observadorDevice = new ObservadorDevice[nCsma]; // Se instancian nCsma objetos de tipo ObservadorDevice

    for (uint32_t i = 0; i < nCsma; ++i)              // Bucle para dar el id de nodo asociado al observador. Para logs
    {
        m_observadorDevice[i].SetIdDevice(i);
    }

    // Suscripciones a las trazas
    for (uint32_t i = 0; i < nCsma; ++i)
    {
        // Proceso de Backoff, finalización del envío de una trama y descarte de transmisión
        csmaDevices->Get (i)->TraceConnectWithoutContext("MacTxBackoff", MakeCallback(&ObservadorDevice::Colision, &m_observadorDevice[i]));
        csmaDevices->Get (i)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&ObservadorDevice::PaqueteEnviado, &m_observadorDevice[i]));
        csmaDevices->Get (i)->TraceConnectWithoutContext("PhyTxDrop", MakeCallback(&ObservadorDevice::PaquetePerdido, &m_observadorDevice[i]));
    }

    for (uint32_t i = 0; i < nCsma-1; ++i) // Se excluye la suscrip. del servidor para los cálculos de tiempos de eco
    {
        // Paquete listo para intentar transmitirlo y paquete listo para ser entregado al nivel de red
        csmaDevices->Get (i)->TraceConnectWithoutContext("MacTx", MakeCallback(&ObservadorDevice::EchoRequest, &m_observadorDevice[i]));
        csmaDevices->Get (i)->TraceConnectWithoutContext("MacRx", MakeCallback(&ObservadorDevice::EchoResponse, &m_observadorDevice[i]));
    }
}


double
Observador::MediaIntentos (uint32_t device)
{
    NS_LOG_FUNCTION (device);
    /* 
        Este método devuelve por nodo (el indicado como argumento) el número medio de intentos necesarios para 
      transmitir efectivamente un paquete.
        
        Para el obtener el dato se llama al método MediaIntentos del ObservadorDevice del nodo en cuestión.
    */

    return m_observadorDevice[device].MediaIntentos();
}

double
Observador::MediaIntentos (uint32_t desde, uint32_t hasta)
{
    NS_LOG_FUNCTION (desde << hasta);
    /* 
        Este método devuelve el número medio de intentos necesarios para transmitir efectivamente un paquete teniendo en
      cuenta el total de paquetes enviados en los nodos indicados.
        
        Para el obtener el dato se calcula la media de las medias de intentos de cada nodo.

        Para que un nodo sea tenido en cuenta en esta media se debe haber transmitido al menos un paquete.

        Si ningún nodo ha transmitido ningún paquete, se devuelve NaN.
    */

    Average<double> acumIntentosTotal;      // Variable local al método
    double mediaIntentos;                   // Variable auxiliar
    for (uint32_t i = desde; i <= hasta; ++i)
    {
        mediaIntentos = m_observadorDevice[i].MediaIntentos();
        if (mediaIntentos != mediaIntentos) // Si NaN... (Un nodo que no ha transmitido ningún paquete no cuenta)
        {
            NS_LOG_DEBUG("NODO " << i << " no ha transmitido ningún paquete. No cuenta para la media de intentos");
        }else{
            acumIntentosTotal.Update(mediaIntentos);
        }
    }

    return acumIntentosTotal.Avg();
}


Time
Observador::MediaEco (uint32_t device)
{
    NS_LOG_FUNCTION (device);
    /* 
        Este método devuelve por nodo (el indicado como argumento) el tiempo medio de eco.
        
        Para el obtener el dato se llama al método MediaEco del ObservadorDevice del nodo en cuestión.
    */

    return m_observadorDevice[device].MediaEco();
}

Time
Observador::MediaEco (uint32_t desde, uint32_t hasta)
{
    NS_LOG_FUNCTION (desde << hasta);
    /* 
        Este método devuelve resultados acerca del tiempo medio de eco del rango de nodos indicados.
        
        Para el obtener el dato se calcula la media de las medias de intentos de cada nodo cliente.

        Si no se han transmitido paquetes en algún nodo el valor del Time devuelto será negativo (conversión de NaN).
    */

    Average<int64_t> acumtEcoTotal; // Variable local al método
    Time mediaEco;                  // Variable auxiliar
    for (uint32_t i = desde; i <= hasta; ++i)
    {
        mediaEco = m_observadorDevice[i].MediaEco();
        if (mediaEco.IsNegative()) // Si el valor de tiempo es negativo... (No cuenta para la media)
        {
            NS_LOG_DEBUG("NODO " << i << " no ha transmitido ningún paquete. No cuenta para la media de tiempo de eco");
        }else{
            acumtEcoTotal.Update(mediaEco.GetMicroSeconds());
        }
    }

    return MicroSeconds( (int64_t) acumtEcoTotal.Avg() );
}


double
Observador::PorcenPaqPerdidos (uint32_t device)
{
    NS_LOG_FUNCTION (device);
    /* 
        Este método devuelve por nodo (el indicado como argumento) el porcentaje de paquetes perdidos (por MaxRetries).
        
        Para el obtener el dato se llama al método PorcenPaqPerdidos del ObservadorDevice del nodo en cuestión.
    */

    return m_observadorDevice[device].PorcenPaqPerdidos();
}

double
Observador::PorcenPaqPerdidos (uint32_t desde, uint32_t hasta)
{
    NS_LOG_FUNCTION (desde << hasta);
    /* 
        Este método devuelve, para el conjunto de nodos (el rango indicado como argumento), el porcentaje de paquetes
      perdidos (por MaxRetries).
        
        Para el obtener el dato se calcula la media de los porcentajes de cada nodo cliente.
    */

    Average<double> acumPorcenPaqPerdidos; // Variable local al método

    for (uint32_t i = desde; i <= hasta; ++i)
    {
        acumPorcenPaqPerdidos.Update(m_observadorDevice[i].PorcenPaqPerdidos());
    }

    return acumPorcenPaqPerdidos.Avg();
}







ObservadorDevice::ObservadorDevice ()
{
    NS_LOG_FUNCTION_NOARGS ();
    /* 
        Constructor de ObservadorDevice. No recibe argumentos ya que hay un objeto de esta clase para cada nodo en la
      topología.

        m_intentos se inicializa a 1 porque un paquete sin colisiones realiza 1 intento.
    */

    m_intentos    = 1;
    m_descartados = 0;
}


void
ObservadorDevice::PaqueteEnviado (Ptr<const Packet> paquete)
{
    NS_LOG_FUNCTION (paquete->GetUid());
    /* 
        Este método es el que captura la traza PhyTxEnd que se da cuando un paquete, tras un cierto número de intentos,
      se consigue enviar completamente.

        Se actualiza el acumulador con el número de intentos necesarios para enviar el paquete. Se resetea el contador
      de intentos a 1 para el siguiente paquete.
    */

    m_acumIntentos.Update(m_intentos);
    NS_LOG_DEBUG ("NODO " << m_idDevice << ": UIDPaq " << paquete->GetUid() << ": " << "Envío tras " << m_intentos << " intentos");

    m_intentos = 1;
}


void
ObservadorDevice::Colision (Ptr<const Packet> paquete)
{
    NS_LOG_FUNCTION (paquete->GetUid());
    /* 
        Este método es el que captura la traza Backoff que se da cuando un paquete detecta una colisión y espera un
        tiempo aleatorio para volver a intentar transmitir.

        Se incrementa en 1 el número de intentos de envío para el paquete en cuestión.
    */

    m_intentos++;
}


double
ObservadorDevice::MediaIntentos ()
{
    NS_LOG_FUNCTION_NOARGS ();
    /* 
        Este método devuelve los intentos en media que se realizan para enviar paquetes del nodo asociado al objeto de
      la clase ObservadorDevice.

        Si el dispositivo no ha enviado ningún paquete, se devolverá "NaN" ya que no se ha podido tomar ninguna medida.
      Es algo improbable si se da el suficiente tiempo simulado.
    */  
    
    return m_acumIntentos.Avg();
}


void
ObservadorDevice::EchoRequest (Ptr<const Packet> paquete)
{
    NS_LOG_FUNCTION (paquete->GetUid());
    /* 
        Este método es el que captura la traza MacTx. Representa la primitiva de petición que el nivel de red le hace al
      nivel de enlace, es decir, cuando el nivel de enlace recibe el paquete de la aplicación cliente.

        Según los parámetros de simulación que se indiquen, podría darse el caso de tener tráfico ICMP. Por ello, y a
      pesar de tener unos condicionales más complejos, se diferencia el tráfico UDP del de otro tipo.

        Supongo que el tráfico UDP será únicamente de las aplicaciones cliente y servidor de eco.

        Tengo en cuenta la suposición indicada en el enunciado: "Cuando el cliente recibe un paquete de eco, éste se
      corresponde con el último paquete enviado, aunque esto no es estrictamente cierto". Es decir, supongo que un
      paquete y su respuesta, si llegan, llegarán antes de que se envíe un nuevo paquete para un funcionamiento correcto.

        La suposición anterior tiene importantes consecuencias en el cálculo de los tiempos de eco. Si se envían dos
      EchoRequest y después llega un EchoResponse (pero del anterior) el tiempo se apreciará como muy corto.

        Supuesto: Se envía EchoRequest (A). No se recibe EchoResp. Se envía una nueva EchoRequest (B). No se contabiliza
      nada referido a (A) por entender que para el cálculo estadístico de tiempo de eco no deben influir los paquetes 
      incorrectamente transmitidos (en este caso (B)).

        Supongo que el tiempo de eco siempre será menor que el intervalo entre solicitudes.

        Si la SDU de nivel de enlace corresponde con una solicitud de eco que se quiere transmitir, se guarda el
      instante en el que la solicitud se produce.
    */

    Ptr<Packet> copia = paquete->Copy ();    // Se obtiene una copia local al método del paquete involucrado en la traza
    EthernetHeader ethHeader;                // Cabecera de nivel de enlace (Ethernet)
    Ipv4Header ipHeader;                     // Cabecera de nivel de red (Ipv4)
    copia->RemoveHeader (ethHeader);         // Desencapsula y obtiene la información de la cabecera Ethernet

    /*
    std::ostringstream info_ethHeader;
    ethHeader.Print(info_ethHeader);
    NS_LOG_DEBUG( info_ethHeader.str() );
    */

    if(ethHeader.GetLengthType() == 0x0800)  // Si el ethertype es el de IP...
    {
        copia->RemoveHeader (ipHeader);      // Desencapsula y obtiene la información de la cabecera Ethernet
        if(ipHeader.GetProtocol() == 17)     // Si el protocolo de transporte encapsulado es UDP...
        {
            m_tRequest = Simulator::Now();   // Se guarda el instante de la solicitud de eco
            NS_LOG_DEBUG ("NODO " << m_idDevice << ": UIDPaq " << paquete->GetUid() << ": " << "Soliditud de Eco en " << m_tRequest);
        }
    }
}


void
ObservadorDevice::EchoResponse (Ptr<const Packet> paquete)
{
    NS_LOG_FUNCTION (paquete->GetUid());
    /* 
        Este método es el que captura la traza MacRx. Se utiliza esta y no PhyRxEnd porque el nivel de enlace recibirá
      algunas tramas que en realidad no serán para ese dispositivo.

        Representa la primitiva de confirmación que el nivel de enlace le hace al nivel de red, es decir, cuando el
      nivel de enlace entrega el paquete de respuesta de eco a la aplicación cliente.

        Tengo en cuenta las suposiciones equivalentes a las tomadas en el método EchoRequest.

        Utilizo el método GetMicroSeconds de Time porque es el que obtiene el tiempo en la unidad de resolución.

        Si el paquete recibido es una respuesta de eco (supongo que es correspondiente a la última solicitud de eco 
      enviada), se comparan los instantes temporales de solicitud-respuesta para obtener el tiempo de eco y se actualiza
      el acumulador.
    */

    Ptr<Packet> copia = paquete->Copy ();
    EthernetHeader ethHeader;
    Ipv4Header ipHeader;
    copia->RemoveHeader (ethHeader);

    if( ethHeader.GetLengthType() == 0x0800)   // Si el ethertype es el de IP...
    {
        copia->RemoveHeader (ipHeader);
        if(ipHeader.GetProtocol() == 17)       // Si el protocolo de transporte encapsulado es UDP...
        {
            Time tResponse = Simulator::Now(); // Variable auxiliar
            m_acumtEco.Update( operator- (tResponse, m_tRequest).GetMicroSeconds() ); // Se actualiza con tiempo de eco
            NS_LOG_DEBUG ("NODO " << m_idDevice << ": UIDPaq " << paquete->GetUid() << ": " << "Respuesta de Eco en " << tResponse);
        }
    }
}


Time
ObservadorDevice::MediaEco ()
{
    NS_LOG_FUNCTION_NOARGS ();
    /*
        Este método devuelve el tiempo medio transcurrido entre el envío de un paquete por un cliente y la recepción
      correcta de eco.

        Se utiliza la conversión a int64_t porque MicroSeconds espera ese tipo. En dicha conversión se truncan los
      decimales, pero como la resolución de Time es del microsegundo, no tiene importancia.

        Si el protocolo no ha completado ningún eco, m_acumtEco.Avg() devolverá "NaN" ya que no se ha podido tomar
      ninguna medida. El valor asignado al Time es inesperado.

    */

    return MicroSeconds( (int64_t) m_acumtEco.Avg() );                                                      
}


void
ObservadorDevice::PaquetePerdido (Ptr<const Packet> paquete)
{
    NS_LOG_FUNCTION (paquete->GetUid());
    /*
        Este método es el que captura la traza PhyTxDrop que se da cuando un paquete alcanza el número máximo de
      intentos de transmisión.

        Se incrementa el contador de paquetes descartados (por MaxRetries) y se pone a 1 el contador de intentos. De
      esta forma, un paquete descartado no influye en el número de intentos necesarios para transmitir un paquete. 
      (Consideración indicada en el enunciado).
    */

    m_descartados++;
    NS_LOG_DEBUG ("NODO " << m_idDevice << ": UIDPaq " << paquete->GetUid() << ": " << "Descarte tras " << m_intentos << " intentos");
    m_intentos = 1;
}


double
ObservadorDevice::PorcenPaqPerdidos ()
{
    NS_LOG_FUNCTION_NOARGS ();
    /*
        Este método devuelve el porcentaje de paquetes perdidos (por MaxRetries).
        
        Para el cálculo, se tienen en cuenta el número de paquetes perdidos frente al número total de paquetes
      (correctos + incorrectos).
    */
    
    uint32_t paqTotales = m_acumIntentos.Count() + m_descartados; // Variable auxiliar

    return 100 * (double) m_descartados / paqTotales;
}


void
ObservadorDevice::SetIdDevice(uint32_t idDevice)
{
    NS_LOG_FUNCTION (idDevice);
    /* 
        Este método asigna un identificador del nodo asociado al observador. Se utiliza para poder identificar mejor los
      logs.
        
        No hay un método Get porque los logs son llamados desde el objeto de la clase ObservadorDevice.
    */

    m_idDevice = idDevice;
}