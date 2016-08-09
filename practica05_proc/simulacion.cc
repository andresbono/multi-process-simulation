/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <unistd.h>
#define LEC 0       // Descriptores de lectura y escritura
#define ESC 1

#include "ns3/object.h"
#include "ns3/global-value.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

#include "ns3/gnuplot.h"
#include "Observador.h"

#define DNI_0     9               // Última cifra del DNI           (DNI: ******59)
#define DNI_1     5               // Penúltima cifra del DNI

#define NCSMA_DEF 10 - DNI_0/2    // Número de nodos por defecto en función del DNI. (Inicialmente era 10)
#define TAMPQ_DEF 500 + 100*DNI_1 // Tamaño de paquetes por defecto en función del DNI. (inicialmente era 1024)

#define TSTOP     150.0           // Tiempo de parada de la simulación (s)

//   Elección del rango del eje de abcisas (Número máximo de reintentos admisibles) para que en la gŕafica se aprecien
// todos los puntos significativos.
#define MRETINI   4               // MaxRETries INIcial
#define INCMRET   1               // INCremento de MaxRETries
#define MRETFIN   16              // MaxRETries FINal

#define SIMPP     15              // Simulaciones por punto (Al menos 10).

#define T_14_025  2.1448          // t_i_j  :  i=n-1  ;  j=(1-p)/2

typedef struct {                  // Struct para el paso de parámetros fijados a la función simulación
    uint32_t nCsma;
    Time     retardoProp;
    DataRate capacidad;
    uint32_t tamPaquete;
    Time     intervalo;
} parametros;

typedef struct {                  // Struct para la devolución de valores de la función simulación
    double nMediaIntentos;
    Time tMediaEco;
    double porcenPaqTxCorrect;
} resultados;


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("practica05");

/*************** Declaración de funciones ***************/
resultados simulacion (uint32_t nMaxRetries, parametros * param);
void logsDebug (uint32_t nCsma, Observador * observador);







int
main (int argc, char *argv[])
{
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(true));
    Time::SetResolution (Time::US);


    /*********** Parámetros por línea de comandos ***********/
    parametros param = {
        .nCsma       = NCSMA_DEF,           // Valor por defecto en función del DNI
        .retardoProp = Time("6560ns"),      // Se truncará a microsegundos (unidad de resolución)
        .capacidad   = DataRate("100Mb/s"),
        .tamPaquete  = TAMPQ_DEF,           // Valor por defecto en función del DNI
        .intervalo   = Time("1s")
    };

    uint32_t maxProc = 1;                   // Número máximo de procesos hijo a crear

    // Se confía en el buen uso del usuario para que no introduzca valores anómalos.
    CommandLine cmd;
    cmd.AddValue ("maxProc", "Número máximo procesos hijo a crear", maxProc);    
    cmd.AddValue ("nCsma", "Número de nodos de la red local", param.nCsma);
    cmd.AddValue ("retardoProp", "Retardo de propagación en el bus", param.retardoProp);
    cmd.AddValue ("capacidad", "Capacidad del bus", param.capacidad);
    cmd.AddValue ("tamPaquete", "Tamaño de las SDU de aplicación", param.tamPaquete);
    cmd.AddValue ("intervalo", "Tiempo entre dos paquetes consecutivos enviados por el mismo cliente", param.intervalo);
    cmd.Parse (argc,argv);
    NS_LOG_FUNCTION (param.nCsma << param.retardoProp << param.capacidad << param.tamPaquete << param.intervalo);


    /*********************** Gráficas ***********************/
    Gnuplot plot_Intentos, plot_Eco, plot_Porcen;                       // 3 gráficas
    Gnuplot2dDataset::SetDefaultStyle (Gnuplot2dDataset::LINES_POINTS); // Se hace una única vez por defecto
    Gnuplot2dDataset::SetDefaultErrorBars(Gnuplot2dDataset::Y);

    plot_Intentos.SetTitle("Número medio de intentos frente a Nº máx. de reintentos admisibles");
    plot_Intentos.SetLegend("MaxRetries", "nMedioIntentos");

    plot_Eco.SetTitle("Tiempo medio hasta recepción del eco frente a Nº máx. de reintentos admisibles");
    plot_Eco.SetLegend("MaxRetries", "tMediaEco (us)");

    plot_Porcen.SetTitle("Porcentaje de paq. transmitidos correctamente frente a Nº máx. de reintentos admisibles");
    plot_Porcen.SetLegend("MaxRetries", "porcenPaqTxCorrect (%)");

    //   Ya que no hay que diferenciar entre curvas dentro de una gráfica, utilizo el título de la curva para indicar
    // los parámetros dependientes del DNI.
    std::ostringstream rotulo;
    rotulo << "nCsma=" << param.nCsma << ", tamPaquete=" << param.tamPaquete << ", SIMPP=" << SIMPP;
    Gnuplot2dDataset datos_Intentos(rotulo.str());
    Gnuplot2dDataset datos_Eco(rotulo.str());
    Gnuplot2dDataset datos_Porcen(rotulo.str());


    /***************** Bucles de simulación *****************/
    resultados res;
    Average<double> acumSim_Intentos;
    Average<int64_t> acumSim_Eco;
    Average<double> acumSim_Porcen;
    double mediaSim_Intentos, mediaSim_Eco, mediaSim_Porcen, z_Intentos, z_Eco, z_Porcen;

    pid_t pid = 1; // Identificador de proceso. También utiliza como variable de control

    for (uint32_t nMaxRetries = MRETINI; pid && nMaxRetries <= MRETFIN; nMaxRetries += INCMRET)
    {
        NS_LOG_INFO ("\n####################### Simulación para MaxRetries=" << nMaxRetries << " #######################");

        /* 
            Bucle de simulaciones modificado para el uso de múltiples procesos. Se crean los procesos hijo mediante fork(),
          se instalan las tuberías y se realiza la simulación en sí por parte de los hijos. También es necesario cambiar
          la semilla del generador de números aleatorios para obtener resultados válidos.
        */
        for (uint32_t i = 0; pid && i < SIMPP; i+=maxProc)
        {
            uint32_t nProc = maxProc; // Número de hilos en cada iteración.

            // Si las simulaciones que quedan son menos que maxProc...
            if (SIMPP - i < maxProc)
            {
                nProc = SIMPP - i;
            }

            int tuberia[nProc][2];                                      // Tabla con los descriptores de cada tubería
            for (uint32_t p = 0; pid && p < nProc; ++p)
            {
                pipe (tuberia[p]);                                      // Creación de tubería
                pid = fork ();                                          // Creación de proceso hijo
                if (pid == 0) // Proceso hijo
                {
                    ns3::RngSeedManager::SetSeed( (uint32_t) getpid()); // Nueva semilla del generador: PID del hijo
                    res = simulacion(nMaxRetries, &param);              // Llamada a simulacion
                    close(tuberia[p][LEC]);
                    write(tuberia[p][ESC], &res, sizeof(res));          // Envío de resultados al proceso padre
                    close(tuberia[p][ESC]);
                }
                else // Proceso padre
                {
                    close (tuberia[p][ESC]);
                }
            }


            for (uint32_t p = 0; pid && p < nProc; ++p)
            {
                read(tuberia[p][LEC],&res,sizeof(res));                 // Recepción de resultados enviados por el hijo

                acumSim_Intentos.Update(res.nMediaIntentos);            // Actualización de acumuladores
                acumSim_Eco.Update(res.tMediaEco.GetMicroSeconds());
                acumSim_Porcen.Update(res.porcenPaqTxCorrect);
            }



        }


        if (pid)
        {
            /*********** Cálculo del IC_0.95 de la medias ***********/
            mediaSim_Intentos = acumSim_Intentos.Mean();                     // Cálculo de medias
            mediaSim_Eco      = acumSim_Eco.Mean();
            mediaSim_Porcen   = acumSim_Porcen.Mean();

            z_Intentos        = T_14_025*sqrt(acumSim_Intentos.Var()/SIMPP); // .Var() devuelve la Cuasivarianza.
            z_Eco             = T_14_025*sqrt(acumSim_Eco.Var()/SIMPP);
            z_Porcen          = T_14_025*sqrt(acumSim_Porcen.Var()/SIMPP);

            acumSim_Intentos.Reset();                                        // Dejar listos para siguiente simulación
            acumSim_Eco.Reset();
            acumSim_Porcen.Reset();

            NS_LOG_INFO ("Intentos: IC_0.95: [ " << mediaSim_Intentos - z_Intentos << " , " << mediaSim_Intentos + z_Intentos << " ]");
            NS_LOG_INFO ("Tiempo de eco: IC_0.95: [ " << mediaSim_Eco - z_Eco << " , " << mediaSim_Eco + z_Eco << " ]");
            NS_LOG_INFO ("Porcentaje Correctos: IC_0.95: [ " << mediaSim_Porcen - z_Porcen << " , " << mediaSim_Porcen + z_Porcen << " ]");

            //            .Add(x, y, error)
            datos_Intentos.Add(nMaxRetries, mediaSim_Intentos, z_Intentos);
            datos_Eco.Add(nMaxRetries, mediaSim_Eco, z_Eco);
            datos_Porcen.Add(nMaxRetries, mediaSim_Porcen, z_Porcen);
        }
    }

    if (pid) // Es el padre
    {
        /****************** Dibujo de gráficas ******************/
        plot_Intentos.AddDataset(datos_Intentos);
        std::ofstream fichero_Intentos("practica05-01.plt");
        plot_Intentos.GenerateOutput(fichero_Intentos);
        fichero_Intentos << "pause -1" << std::endl;
        fichero_Intentos.close();

        plot_Eco.AddDataset(datos_Eco);
        std::ofstream fichero_Eco("practica05-02.plt");
        plot_Eco.GenerateOutput(fichero_Eco);
        fichero_Eco << "pause -1" << std::endl;
        fichero_Eco.close();

        plot_Porcen.AddDataset(datos_Porcen);
        std::ofstream fichero_Porcen("practica05-03.plt");
        plot_Porcen.GenerateOutput(fichero_Porcen);
        fichero_Porcen << "pause -1" << std::endl;
        fichero_Porcen.close();
    }

    return 0;
}







resultados
simulacion (uint32_t nMaxRetries, parametros * param)
{
    NS_LOG_FUNCTION (nMaxRetries); // Sólo con el argumento que es variable
    /*
        Función donde se programa la gestión principal de una simulación simple.
    */
    
    uint32_t nCsma = param->nCsma; // Variable auxiliar


    /******** Montaje y configuración de la topología *******/
    NodeContainer csmaNodes;
    csmaNodes.Create (nCsma);

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", DataRateValue (param->capacidad));
    csma.SetChannelAttribute ("Delay", TimeValue (param->retardoProp));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install (csmaNodes);
    // Configuramos parámetros de backoff en todos los nodos del escenario
    for (uint32_t i = 0; i < nCsma; ++i)
    {
        //                  Conv. del obj. devuelto     SetBackoffParams (slotTime, minSlots, maxSlots, ceiling, MaxRetries)
        csmaDevices.Get(i)->GetObject<CsmaNetDevice>()->SetBackoffParams (Time ("1us"), 10, 1000, 10, nMaxRetries);
    }
    // Instalamos la pila TCP/IP en todos los nodos
    InternetStackHelper stack;
    stack.Install (csmaNodes);
    // Y les asignamos direcciones
    Ipv4AddressHelper address;
    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces = address.Assign (csmaDevices);

    /////////// Instalación de las aplicaciones
    // Servidor
    UdpEchoServerHelper echoServer (9); // Puerto de escucha del servidor (9).
    ApplicationContainer serverApp = echoServer.Install (csmaNodes.Get (nCsma - 1)); // El servidor es el último
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (TSTOP));
    // Clientes
    UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma - 1), 9); // IP_dest (Servidor), Pto_dest (9)
    echoClient.SetAttribute ("MaxPackets", UintegerValue (10000));
    echoClient.SetAttribute ("Interval", TimeValue (param->intervalo));
    echoClient.SetAttribute ("PacketSize", UintegerValue (param->tamPaquete)); // + 8 (UDP) + 20 (IP) + 18 (Eth) + 2 (Phy)?
    NodeContainer clientes;
    for (uint32_t i = 0; i < nCsma - 1; i++)
    {
        clientes.Add (csmaNodes.Get (i));
    }
    ApplicationContainer clientApps = echoClient.Install (clientes);
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (TSTOP));

    // Cálculo de rutas
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


    /************ Captura de datos de simulación ************/
    Observador observador(&csmaDevices); // Un único observador que obtiene datos de todos los nodos de la topología
    // csma.EnablePcap ("practica05", csmaDevices.Get (nCsma - 1), true); // Se comenta tras terminar la depuración

    Simulator::Run ();
    Simulator::Destroy ();

    logsDebug(nCsma, &observador); // Impresión de NS_LOG_DEBUG

    resultados res = {             // Devolución de valores como estructura. Se excluye el nodo 0 y el servidor nCsma-1
        .nMediaIntentos = observador.MediaIntentos (1, nCsma-2),
        .tMediaEco = observador.MediaEco (1, nCsma-2),  
        .porcenPaqTxCorrect = 100.0 - observador.PorcenPaqPerdidos (1, nCsma-2) // Correc % = 100 - Perdidos %
    };
    return res;
}







void
logsDebug (uint32_t nCsma, Observador * observador)
{
    NS_LOG_FUNCTION_NOARGS (); // No nos interesa ningún argumento
    /*
        Si se solicita (ajustando la variable de entorno NS_LOG con practica05=level_debug o superior), se mostrarán
      resultados acerca de la simulación.
    */

    // Número medio de intentos
    for (uint32_t i = 0; i < nCsma -1; ++i) // Valor medio para cada cliente (servidor no)
    {
        NS_LOG_DEBUG ("NODO " << i << " - Intentos promedio por paquete: " << observador->MediaIntentos(i) );
    }
    // NS_LOG_INFO("NODO Servidor - Intentos promedio por paquete: " << observador->MediaIntentos(nCsma -1) );
    NS_LOG_DEBUG ("Intentos promedio por paquete en la topología: " << observador->MediaIntentos(0,nCsma-1) << "\n" );

    // Tiempo medio de eco
    for (uint32_t i = 0; i < nCsma -1; ++i) // Valor medio para cada cliente (servidor no)
    {
        NS_LOG_DEBUG ("NODO " << i << " - Tiempo medio de eco: " << observador->MediaEco(i) );
    }
    NS_LOG_DEBUG ("Tiempo medio de eco de todos los clientes: " << observador->MediaEco(0,nCsma-2) << "\n" );

    // Porcentaje de paquetes perdidos
    for (uint32_t i = 0; i < nCsma; ++i) // Porcentaje para cada nodo
    {
        NS_LOG_DEBUG ("NODO " << i << " - Porcentaje de paquetes perdidos: " << observador->PorcenPaqPerdidos(i) << "%" );
    }
    NS_LOG_DEBUG ("Porcentaje de paquetes perdidos de todos los clientes: " << observador->PorcenPaqPerdidos(0,nCsma-2) << "%" );
    NS_LOG_DEBUG ("Porcentaje de paquetes perdidos de la topología: " << observador->PorcenPaqPerdidos(0,nCsma-1) << "%" << "\n" );
}