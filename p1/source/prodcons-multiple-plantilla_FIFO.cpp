#include <iostream>
#include <cassert>
#include <thread>
#include <mutex>
#include <random>
#include "scd.h"

using namespace std ;
using namespace scd ;

//**********************************************************************
// Variables globales
const unsigned 
   num_prod = 3,
   num_cons = 2;
const unsigned 
   num_items = 40 ,   // número de items
	tam_vec   = 10 ;   // tamaño del buffer
unsigned  
   cont_prod[num_items] = {0}, // contadores de verificación: para cada dato, número de veces que se ha producido.
   cont_cons[num_items] = {0}, // contadores de verificación: para cada dato, número de veces que se ha consumido.
   siguiente_dato       = 0 ;  // siguiente dato a producir en 'producir_dato' (solo se usa ahí)
unsigned
   buffer[tam_vec] = {0}, // vector intermedio (buffer de comunicación entre hebras)
   primera_libre = 0,     // "puntero" a la posición donde producir datos del buffer
   primera_ocupada = 0;   // "puntero" a la posición donde consumir datos del buffer
Semaphore
   libres (tam_vec),      // veces que se puede escribir en el buffer antes del bloqueo
   ocupadas (0);          // veces que se puede leer el buffer (segun escrituras)

mutex
   cerrojo_prod,
   cerrojo_cons,
   cerrojo;

//**********************************************************************
// funciones comunes a las dos soluciones (fifo y lifo)
//----------------------------------------------------------------------

unsigned producir_dato()
{
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ));

   cerrojo.lock();
   const unsigned dato_producido = siguiente_dato ;
   siguiente_dato++ ;
   cerrojo.unlock();

   cont_prod[dato_producido] ++ ;
   cout << "producido: " << dato_producido << endl << flush ;
   return dato_producido ;
}
//----------------------------------------------------------------------

void consumir_dato( unsigned dato )
{
   assert( dato < num_items );
   cont_cons[dato] ++ ;
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ));

   cout << "                  consumido: " << dato << endl ;

}


//----------------------------------------------------------------------

void test_contadores()
{
   bool ok = true ;
   cout << "comprobando contadores ...." ;
   for( unsigned i = 0 ; i < num_items ; i++ )
   {  if ( cont_prod[i] != 1 )
      {  cout << "error: valor " << i << " producido " << cont_prod[i] << " veces." << endl ;
         ok = false ;
      }
      if ( cont_cons[i] != 1 )
      {  cout << "error: valor " << i << " consumido " << cont_cons[i] << " veces" << endl ;
         ok = false ;
      }
   }
   if (ok)
      cout << endl << flush << "solución (aparentemente) correcta." << endl << flush ;
}

//----------------------------------------------------------------------
/**
 * @brief produce una lista de numeros de 0 a num_items-1.
 * Además permite a una hebra (o varias) producir tantos datos seguidos como quepan
 * en un buffer para luego ser consumidos, avisa a la hebra consumidora cuando hay
 * un dato para consumir
 * @param i integer. = 0 por defecto, idenficador de la hebra que produce 
 * 
 */
void  funcion_hebra_productora( unsigned i )
{
   // Asignar estaticamente el numero de elementos que va a ejecutar cada hebra
   unsigned items_por_productor = num_items / num_prod; 
   unsigned sobrantes = num_items % num_prod; 

    // Repartir el numero de iteraciones sobrantes en cada hebra
    unsigned inicio = i * items_por_productor + (i < sobrantes ? i : sobrantes);
    unsigned fin = inicio + items_por_productor + (i < sobrantes ? 1 : 0);

   for( unsigned j = inicio; j < fin; ++j ){

      sem_wait(libres) ;
      int dato = producir_dato() ;

      buffer[primera_libre] = dato ;
      cerrojo_prod.lock() ;
      primera_libre = (primera_libre + 1) % tam_vec ;
      cerrojo_prod.unlock() ;

      sem_signal(ocupadas) ;
   }
}

//----------------------------------------------------------------------
/**
 * @brief consume una lista de numeros de 0 a num_items-1.
 * Además permite a una hebra (o varias) consumir tantos datos seguidos como quepan
 * en un buffer luego de ser producidos, avisa a la hebra productora cuando hay
 * un hueco para producir
 * @param i intenger. = 0 por defecto, identicador de la hebra que consume
 * 
 */
void funcion_hebra_consumidora( unsigned i )
{
   // Asignar estaticamente el numero de elementos que va a ejecutar cada hebra
   unsigned items_por_productor = num_items / num_cons; 
   unsigned sobrantes = num_items % num_cons; 

   // Repartir el numero de iteraciones sobrantes en cada hebra
   unsigned inicio = i * items_por_productor + (i < sobrantes ? i : sobrantes);
   unsigned fin = inicio + items_por_productor + (i < sobrantes ? 1 : 0);

   for( unsigned j = inicio; j < fin; ++j )
   {
      sem_wait(ocupadas);
      int dato ;
      
      dato = buffer[primera_ocupada] ; 
      cerrojo_cons.lock() ;
      primera_ocupada = (primera_ocupada + 1) % tam_vec ;
      cerrojo_cons.unlock() ;
      
      consumir_dato( dato ) ;
      
      sem_signal(libres);
      
    }
}
//----------------------------------------------------------------------

int main()
{
   cout << "------------------------------------------------------------------------------------------" << endl
        << "Problema de los productores-consumidores ( solución FIFO ). Multiples productor-consumidor" << endl
        << "------------------------------------------------------------------------------------------" << endl
        << flush ;

   thread hebras_productoras[num_prod];
   thread hebras_consumidoras[num_cons];

   // Inicialización de las hebras productoras
   for (int i = 0; i < num_prod; ++i) {
      hebras_productoras[i] = thread(funcion_hebra_productora, i);
   }

   // Inicialización de las hebras consumidoras
   for (int i = 0; i < num_cons; ++i) {
      hebras_consumidoras[i] = thread(funcion_hebra_consumidora, i);
   }

   // Hacer join con las hebras productoras
   for (int i = 0; i < num_prod; ++i) {
      hebras_productoras[i].join();
   }

   // Hacer join con las hebras consumidoras
   for (int i = 0; i < num_cons; ++i) {
      hebras_consumidoras[i].join();
   }

   test_contadores();
}
