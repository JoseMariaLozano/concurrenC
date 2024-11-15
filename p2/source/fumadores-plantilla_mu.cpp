#include <iostream>
#include <cassert>
#include <thread>
#include <mutex>
#include <random> // dispositivos, generadores y distribuciones aleatorias
#include <chrono> // duraciones (duration), unidades de tiempo
#include "../include/scd.h"

#define MOSTRADOR_VACIO -1

using namespace std ;
using namespace scd ;

// numero de fumadores 

const int num_fumadores = 3 ;

class Papelera : public HoareMonitor
{
private:
static const int           // constantes ('static' ya que no dependen de la instancia)
    num_celdas_total = 10;   //   núm. de entradas del buffer
 int                        // variables permanentes
    buffer[num_celdas_total],//   buffer de tamaño fijo, con los datos
    primera_libre ;          //   indice de celda de la próxima inserción

 CondVar                    // colas condicion:
    ocupadas,                //  cola donde espera el consumidor (primera_ocupada>0)
    libres ;                 //  cola donde espera el productor  (primera_libre<num_celdas_total)

 public:                    // constructor y métodos públicos
    Papelera() ;             // constructor
    void  recoger_colillas();                // extraer un valor (sentencia L) (consumidor)
    void tirar_colillas( int valor ); // insertar un valor (sentencia E) (productor) 

};

Papelera::Papelera()
{
    primera_libre = 0;
    for(size_t i = 0; i < num_celdas_total; i++)
    {
        buffer[i] = 0;
    }
    ocupadas = newCondVar();
    libres = newCondVar();
}

void Papelera::recoger_colillas()
{
    // esperar bloqueado hasta que primera_libre != primera_ocupada (haya algo que leer)
   if ( primera_libre == 0 )
      ocupadas.wait();

   //cout << "leer: ocup == " << primera_libre << ", total == " << num_celdas_total << endl ;
   assert( 0 < primera_libre ); // tener cuidado que no sean iguales para no leer algo ya leído o no producido

   // hacer la operación de lectura, actualizando estado del monitor
   primera_libre--;
   const int valor = buffer[primera_libre] ;
   
   
   // señalar al productor que hay un hueco libre, por si está esperando
   libres.signal();

   // devolver valor
   cout << "Reciclado un nuevo valor " << valor << endl; 
}

void Papelera::tirar_colillas(int valor) 
{
    // esperar bloqueado hasta que primera_libre+1 no sea == primera_ocupada (haya espacio para escribir sin borrar algo ya escrito)
   if ( primera_libre == num_celdas_total )
      libres.wait();

   //cout << "escribir: ocup == " << primera_libre << ", total == " << num_celdas_total << endl ;
   assert( primera_libre < num_celdas_total ); // puede no ser igual y ser mayor, si es mayor no podemos escribir

   // hacer la operación de inserción, actualizando estado del monitor
   buffer[primera_libre] = valor ;
   primera_libre++ ;

   // señalar al consumidor que ya hay una celda ocupada (por si esta esperando)
   ocupadas.signal();
}


// Own implementation for lectores escritores (no usamos el buffer pero podría usarse)
class Estanco : public HoareMonitor
{
private:    
int 
    ingrediente_mostrador;             // number os readers reading in an exact moment
CondVar 
    fumadores[num_fumadores],
    mostrador_vacio; 

public:
    Estanco();
    void poner_ingrediente(int ingrediente);
    void coger_ingrediente(int num_fumador);
};

Estanco::Estanco()
{
    ingrediente_mostrador = MOSTRADOR_VACIO;
    for(size_t i = 0; i < num_fumadores; i++)
    {
        fumadores[i] = newCondVar();
    }
    mostrador_vacio = newCondVar();
}

void Estanco::poner_ingrediente(int ingrediente)
{
    if(ingrediente_mostrador != MOSTRADOR_VACIO)
    {
        mostrador_vacio.wait();
    }

    ingrediente_mostrador = ingrediente;
    cout << "\tPuesto ingrediente " << ingrediente << " en el mostrador" << endl;
    fumadores[ingrediente_mostrador].signal();
}

void Estanco::coger_ingrediente(int num_fumador)
{
    if(ingrediente_mostrador != num_fumador)
    {
        fumadores[num_fumador].wait();
    }

    ingrediente_mostrador = MOSTRADOR_VACIO;
    cout << "Retirado ingrediente: " << num_fumador << " del mostrador" << endl;
    mostrador_vacio.signal();
}



//-------------------------------------------------------------------------
// Función que simula la acción de producir un ingrediente, como un retardo
// aleatorio de la hebra (devuelve número de ingrediente producido)

int producir_ingrediente()
{
   // calcular milisegundos aleatorios de duración de la acción de fumar)
   chrono::milliseconds duracion_produ( aleatorio<10,100>() );

   // informa de que comienza a producir
   cout << "Estanquero : empieza a producir ingrediente (" << duracion_produ.count() << " milisegundos)" << endl;

   // espera bloqueada un tiempo igual a ''duracion_produ' milisegundos
   this_thread::sleep_for( duracion_produ );

   const int num_ingrediente = aleatorio<0,num_fumadores-1>() ;

   // informa de que ha terminado de producir
   cout << "Estanquero : termina de producir ingrediente " << num_ingrediente << endl;

   return num_ingrediente ;
}


//----------------------------------------------------------------------
// función que ejecuta la hebra del estanquero

void funcion_hebra_estanquero( MRef<Estanco> monitor )
{
   int ingrediente;
   while(true) 
   {  
      ingrediente = producir_ingrediente();
      monitor->poner_ingrediente(ingrediente);
   }
}

//-------------------------------------------------------------------------
// Función que simula la acción de fumar, como un retardo aleatoria de la hebra

void fumar( int num_fumador )
{

   // calcular milisegundos aleatorios de duración de la acción de fumar)
   chrono::milliseconds duracion_fumar( aleatorio<20,200>() );

   // informa de que comienza a fumar

    cout << "Fumador " << num_fumador << "  :"
          << " empieza a fumar (" << duracion_fumar.count() << " milisegundos)" << endl;

   // espera bloqueada un tiempo igual a ''duracion_fumar' milisegundos
   this_thread::sleep_for( duracion_fumar );

   // informa de que ha terminado de fumar

    cout << "\tFumador " << num_fumador << "  : termina de fumar, comienza espera de ingrediente." << endl;

}

//----------------------------------------------------------------------
// función que ejecuta la hebra del fumador
void  funcion_hebra_fumador( int num_fumador, MRef<Estanco> monitor, MRef<Papelera> papelera )
{
   while( true )
   {
      monitor->coger_ingrediente( num_fumador );
      fumar(num_fumador);
      papelera->tirar_colillas(num_fumador);

   }
}

//----------------------------------------------------------------------
// funcion que ejecuta la hebra recicladora
void funcion_hebra_recicladora( MRef<Papelera> papelera )
{
    while (true)
    {
        papelera->recoger_colillas();
    }
}

int main()
{
    cout << "--------------------------" << endl
        << "Problema de los fumadores " << endl
        << "--------------------------" << endl
        << flush ;
    

    MRef<Estanco> monitor = Create<Estanco>() ;
    MRef<Papelera> papelera = Create<Papelera>() ;


    thread fumadores[num_fumadores];
    thread estanquero(funcion_hebra_estanquero, monitor);
    thread recicladora(funcion_hebra_recicladora, papelera);
   
     // Inicialización de las hebras productoras
    for (int i = 0; i < num_fumadores; ++i) {
        fumadores[i] = thread(funcion_hebra_fumador, i, monitor, papelera);
    }

   // Hacer join con las hebras productoras
    for (int i = 0; i < num_fumadores; ++i) {
        fumadores[i].join();
    }
   
    estanquero.join();

}
