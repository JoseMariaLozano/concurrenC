#include "../include/scd.h"
#include <chrono>
#include <random>
#include <iostream>
#include <thread>

// using namespace std::chrono;
using namespace std;
using namespace scd;

mutex
    mtx;
constexpr int
    min_ms = 5,
    max_ms = 20,
    num_lec = 2,
    num_esc = 2;
string
    mensaje_inicio = "inicio",
    mensaje_final  = "final";

// similar to producir and consumir dato functions
void retraso_aleatorio_lectura( string mensaje, unsigned n_lectora) 
{
   this_thread::sleep_for( chrono::milliseconds( aleatorio<min_ms,max_ms>() ));
   mtx.lock();
   cout << "Hebra lectora: " << n_lectora << endl;
   cout << "\t Haciendo tareas de lectura ( " << mensaje << " )" << endl ;
   mtx.unlock();
}

// similar to producir and consumir dato functions
void retraso_aleatorio_escritura(string mensaje, unsigned n_escritora) 
{
   this_thread::sleep_for( chrono::milliseconds( aleatorio<min_ms,max_ms>() ));
   mtx.lock();
      cout << "Hebra escritora: " << n_escritora << endl;
   cout << "\t Haciendo tareas de escritura ( " << mensaje << " )" << endl ;
   mtx.unlock();
}

// Own implementation for lectores escritores (no usamos el buffer pero podría usarse)
class Lec_Esc : public HoareMonitor
{
private:
static const unsigned 
    num_items = 10;    // static const to represent the size of the buffer
unsigned 
    buffer[num_items], // unsigned int buffer for reading/writing purposes
    n_lec;             // number os readers reading in an exact moment
CondVar 
    lectura,
    escritura; // condition variable to control read/write waitings
bool
    escrib; // true if a writer is writing false otherwise

public:
    Lec_Esc();
    unsigned ini_lectura();
    unsigned fin_lectura();
    void ini_escritura();
    void fin_escritura();
};

Lec_Esc::Lec_Esc()
{
    n_lec = 0;
    escrib = false;
    lectura = newCondVar();
    escritura = newCondVar();
}

unsigned Lec_Esc::ini_lectura()
{
    if (escrib)
        lectura.wait();
    
    n_lec = n_lec + 1;

    lectura.signal();

    return 0;
}

unsigned Lec_Esc::fin_lectura()
{
    n_lec = n_lec - 1;

    if (n_lec == 0) 
        escritura.signal();
    
    return 0;
}

void Lec_Esc::ini_escritura()
{
    if ((n_lec > 0) || escrib)
        escritura.wait();
    
    escrib = true;
}

void Lec_Esc::fin_escritura()
{
    escrib = false;

    if (not lectura.empty())
        lectura.signal();
    else
        escritura.signal();
}

// No hace nada mas que un mensaje por pantalla con un retraso aleatorio
void funcion_hebra_lectura( MRef<Lec_Esc> monitor, unsigned i )
{
    while (true)
    {
        monitor->ini_lectura();
        retraso_aleatorio_lectura(mensaje_inicio, i);
        monitor->fin_lectura();
        retraso_aleatorio_lectura(mensaje_final, i);
    }
}

// No hace nada mas que un mensaje por pantalla con un retraso aleatorio
void funcion_hebra_escritora( MRef<Lec_Esc> monitor , unsigned i)
{
    while (true)
    {
        monitor->ini_escritura();
        retraso_aleatorio_escritura(mensaje_inicio, i);
        monitor->fin_escritura();
        retraso_aleatorio_escritura(mensaje_final, i);
    }
}

int main ()
{
    cout << "-----------------------------------------------------" << endl
         << "Problema del lector-escritor multiples (Monitor SU). " << endl
         << "-----------------------------------------------------" << endl
         << flush ;

    MRef<Lec_Esc> monitor = Create<Lec_Esc>() ;
    
    thread hebras_lectoras[num_lec];
    thread hebras_escritoras[num_esc];

    // Inicialización de las hebras productoras
    for (unsigned i = 0; i < num_lec; ++i) 
    {
        hebras_lectoras[i] = thread(funcion_hebra_lectura, monitor, i);
    }

    // Inicialización de las hebras consumidoras
    for (unsigned i = 0; i < num_esc; ++i) 
    {
        hebras_escritoras[i] = thread(funcion_hebra_escritora, monitor, i);
    }

    // Hacer join con las hebras productoras
    for (unsigned i = 0; i < num_lec; ++i) 
    {
        hebras_lectoras[i].join();
    }

    // Hacer join con las hebras consumidoras
    for (unsigned i = 0; i < num_esc; ++i) 
    {
        hebras_escritoras[i].join();
    }

    return 0;
}
