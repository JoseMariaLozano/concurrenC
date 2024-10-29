#include <iostream>
#include <iomanip>
#include <cassert>
#include <random>
#include <thread>
#include "../include/scd.h"

using namespace std ;
using namespace scd ;

constexpr int
   num_items = 15 , // número de items a producir/consumir
   num_cons = 2,
   num_prod = 2;   
int
   siguiente_dato = 0 ; // siguiente valor a devolver en 'producir_dato'
   
constexpr int               
   min_ms    = 5,     // tiempo minimo de espera en sleep_for
   max_ms    = 20 ;   // tiempo máximo de espera en sleep_for


mutex
   mtx ;                 // mutex de escritura en pantalla
unsigned
   cont_prod[num_items] = {0}, // contadores de verificación: producidos
   cont_cons[num_items] = {0}; // contadores de verificación: consumidos

//**********************************************************************
// funciones comunes a las dos soluciones (fifo y lifo)
//----------------------------------------------------------------------

int producir_dato(  )
{
   
   this_thread::sleep_for( chrono::milliseconds( aleatorio<min_ms,max_ms>() ));
   const int valor_producido = siguiente_dato ;
   siguiente_dato ++ ; // este incremento parece una seccion crítica 
   mtx.lock();
   cout << "hebra productora, produce " << valor_producido << endl << flush ;
   mtx.unlock();
   cont_prod[valor_producido]++ ;
   return valor_producido ;
}
//----------------------------------------------------------------------

void consumir_dato( unsigned valor_consumir )
{
   if ( num_items <= valor_consumir )
   {
      cout << " valor a consumir === " << valor_consumir << ", num_items == " << num_items << endl ;
      assert( valor_consumir < num_items );
   }
   cont_cons[valor_consumir] ++ ;
   this_thread::sleep_for( chrono::milliseconds( aleatorio<min_ms,max_ms>() ));
   mtx.lock();
   cout << "                  hebra consumidora, consume: " << valor_consumir << endl ;
   mtx.unlock();
}
//----------------------------------------------------------------------

void test_contadores()
{
   bool ok = true ;
   cout << "comprobando contadores ...." << endl ;

   for( unsigned i = 0 ; i < num_items ; i++ )
   {
      if ( cont_prod[i] != 1 )
      {
         cout << "error: valor " << i << " producido " << cont_prod[i] << " veces." << endl ;
         ok = false ;
      }
      if ( cont_cons[i] != 1 )
      {
         cout << "error: valor " << i << " consumido " << cont_cons[i] << " veces" << endl ;
         ok = false ;
      }
   }
   if (ok)
      cout << endl << flush << "solución (aparentemente) correcta." << endl << flush ;
}

// *****************************************************************************
// clase para monitor buffer, version FIFO, semántica SC, multiples prod/cons

class ProdConsSU1 : public HoareMonitor
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
   ProdConsSU1() ;             // constructor
   int  leer();                // extraer un valor (sentencia L) (consumidor)
   void escribir( int valor ); // insertar un valor (sentencia E) (productor)
} ;
// -----------------------------------------------------------------------------

ProdConsSU1::ProdConsSU1(  )
{
   primera_libre = 0 ;
   ocupadas      = newCondVar();
   libres        = newCondVar();
}
// -----------------------------------------------------------------------------
// función llamada por el consumidor para extraer un dato

int ProdConsSU1::leer(  )
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
   return valor ;
}
// -----------------------------------------------------------------------------

void ProdConsSU1::escribir( int valor )
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
// *****************************************************************************
// funciones de hebras

void funcion_hebra_productora( MRef<ProdConsSU1> monitor, int i )
{
   unsigned items_por_productor = num_items / num_cons; 
   unsigned sobrantes = num_items % num_cons; 

   // Repartir el numero de iteraciones sobrantes en cada hebra
   unsigned inicio = i * items_por_productor + (i < sobrantes ? i : sobrantes);
   unsigned fin = inicio + items_por_productor + (i < sobrantes ? 1 : 0);

   for( unsigned i = inicio ; i < fin ; i++ )
   {
      int valor = producir_dato(  ) ;
      monitor->escribir( valor );
   }
}
// -----------------------------------------------------------------------------

void funcion_hebra_consumidora( MRef<ProdConsSU1>  monitor, int i )
{
   unsigned items_por_productor = num_items / num_cons; 
   unsigned sobrantes = num_items % num_cons; 

   // Repartir el numero de iteraciones sobrantes en cada hebra
   unsigned inicio = i * items_por_productor + (i < sobrantes ? i : sobrantes);
   unsigned fin = inicio + items_por_productor + (i < sobrantes ? 1 : 0);

   for( unsigned i = inicio ; i < fin ; i++ )
   {
      int valor = monitor->leer();
      consumir_dato( valor ) ;
   }
}
// -----------------------------------------------------------------------------

int main()
{
   cout << "-----------------------------------------------------------------------" << endl
        << "Problema del productor-consumidor multiples (Monitor SU, buffer LIFO). " << endl
        << "-----------------------------------------------------------------------" << endl
        << flush ;

   // crear monitor  ('monitor' es una referencia al mismo, de tipo MRef<...>)
   MRef<ProdConsSU1> monitor = Create<ProdConsSU1>() ;

   thread hebras_productoras[num_prod];
   thread hebras_consumidoras[num_cons];

   // Inicialización de las hebras productoras
   for (int i = 0; i < num_prod; ++i) {
      hebras_productoras[i] = thread(funcion_hebra_productora, monitor, i);
   }

   // Inicialización de las hebras consumidoras
   for (int i = 0; i < num_cons; ++i) {
      hebras_consumidoras[i] = thread(funcion_hebra_consumidora, monitor, i);
   }

   // Hacer join con las hebras productoras
   for (int i = 0; i < num_prod; ++i) {
      hebras_productoras[i].join();
   }

   // Hacer join con las hebras consumidoras
   for (int i = 0; i < num_cons; ++i) {
      hebras_consumidoras[i].join();
   }

   test_contadores() ;
}
