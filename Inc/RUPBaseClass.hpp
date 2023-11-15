/*
   Проект "Серводвигатель для роботов Zubr"
   Автор
     Сибилев А.С.
   Описание
     StMessageZubr - реализация протокола сообщений с сервомашиной собственной разработки Zubr

     Учитывая требования высокой скорости передачи, прежние алгоритмы приема и отправки сообщений
     включая их детектирование - не годятся. Используемая система работы по прерываниям не успевает
     обработать поступающие байты. Было принято решение задействовать систему dma.

     Для системы приема по dma характерна необходимость заранее знать длительность данных, что
     в условиях протокола переменной длины не возможно. Поэтому прием ведется в кольцевой буфер
     постоянно, а программа в своем темпе пытается анализировать принятые данные. В этой связи
     актуальным делается вопрос обозначения начала фрейма (посылки, сообщения). Прежняя система
     детектирования пауз между посылками представляется нереальной для реализации. Было принято
     решение старший бит в байтах отдать под сигнал команда-данные. Это решает вопрос с определением
     начала посылки, но несколько усложняет упаковку данных.

     Таким образом сообщение теперь представляет собой посылку битов, упакованных в байты таким
     образом, что первый байт сообщения имеет в старшем бите 0, а остальные байты сообщения
     имеют в старшем бите 1.

     На передачу
       - заголовок: код команды и идентификатор двигателя
       - данные: один или несколько байтов данных
     На прием
       - заголовок:  код команды и идентификатор двигателя
       - данные: один или несколько байтов данных

       Заголовок
       7 6 5 4  3 2 1 0
       0 -cmd-  --id---

       Команды cmd:
       0 - команда управления, отправка данных воздействия 16бит и получение данных состояния 2*16бит
       1 - получить данные состояния 3*16бит
       2 - резерв
       3
       4
       5 - записать параметр, индекс параметра 16бит, значение параметра 32бит
       6 - прочитать параметр, индекс параметра 16бит
       7 - прошивка, адрес прошивки 32бит, данные прошивки 32бит

       Идентификатор мотора id. Поддерживается 15 двигателей. Двигатель с id = 0 значение по сбросу,
       двигатель с id = 15 универсальный идентификатор для прошивки

       [0] Команда управления:
         Заголовок, Воздействие 2 байт, КС
       ответ
         Текущий угол 2 байт, текущий момент 2 байт, КС

       [1] Получить данные состояния
         Заголовок, КС
       ответ
         3*16 значений состояния, КС

       [5] Записать параметр:
         Заголовок, Индекс параметра (2байт), Параметр (4байта), КС
       ответ
         Параметр (4байта), КС

       [6] Прочитать параметр:
         Заголовок, Индекс параметра (2байт), КС
       ответ
         Параметр (4байта), КС

       [7] Прошивка
         В режиме прошивки заголовок равен 0x7f
         Заголовок 4 байта адрес, 4 байта слово, КС
       ответ
         1 байт - идентификатор устройства, в старших битах которого код ошибки или 0, если ошибок нету
                  0000 -id- в поле id возвращается идентификатор устройства
                   -+-
                    +------ код состояния (0-7)
                            0 - нету ошибок
         4 байта   значение или адрес





   Учитывая, что один из вариантов обмена с хостом - это uart, то целесообразно
   унифицировать обмен между хостом и контроллером.
   Поскольку контроллер один и различие по идентификатору не требуется, решено
   отдать под код команды весь заголовок.

       Заголовок
       7 6 5 4  3 2 1 0
       0 ----cmd-------

   Конкретные коды команд и состав посылок представлены в файле roboComBook.h

 История
   12.01.2023  v1 начал вести версии
   */
#ifndef CSMESSAGE_H
#define CSMESSAGE_H

#include <stdint.h>

//Версия сообщения
#define CS_MESSAGE_VERSION     1

//Команды
#define CS_CMD_MSG_CONTROL     0    //!< Управление 16бит, возвращает состояние 2*16бит
#define CS_CMD_MSG_INFO        1    //!< Получить информацию, возвращает набор параметров 3*16бит
#define CS_CMD_MSG_WRITE       5    //!< Запись параметра (индекс 16бит, значение 32бит, возвращает записанное значение 32бит)
#define CS_CMD_MSG_READ        6    //!< Чтение параметра (индекс 16бит, возвращает значение 32бит)
#define CS_CMD_MSG_FLASH       7    //!< Прошивка (адрес 32бит, значение 32бит)

//Длина сообщения прошивки
#define CS_CMD_FLASH_LENGTH   12

//                             CTRL INFO RSV        WR RD FLASH
//                              0    1    2  3  4    5  6  7
#define CS_CMD_LENGHTS        { 5,   2,   0, 0, 0,   9, 5, CS_CMD_FLASH_LENGTH } //!< Длины команд


//Сигнатуры устройств
#define CS_SIGNATURE_CONFIG 1939 //!< Сигнатура конфигурации
#define CS_SIGNATURE_MFLASH 1940 //!< Загрузчик для серводвигателей в металлическом и пластиковом корпусах
#define CS_SIGNATURE_MOTOR  1946 //!< Стандартный двигатель в металлическом корпусе
#define CS_SIGNATURE_LMOTOR 1948 //!< Двигатель в пластиковом корпусе
#define CS_SIGNATURE_TENSO  1812
#define CS_SIGNATURE_FORCE  1905 //!< Датчик усилия

//Коды ошибок прошивки
#define CS_UE_NONE             0 //!< Нету ошибок
#define CS_UE_SWITCH           1 //!< Ошибка переключения в режим прошивки
#define CS_UE_ERASE            2 //!< Ошибка стирания памяти
#define CS_UE_FLASH          100 //!< Адрес ошибки прошивки


//Кодовая книга
//Для всех устройств
#define CS_CB_SIGNATURE        0 //!< Сигнатура устройства
#define CS_CB_VERSION          1 //!< Версия программы
#define CS_CB_DEVICE_ID        2 //!< Идентификатор устройства
#define CS_CB_PROTOCOL_ID      3 //!< Идентификатор протокола обмена с устройством
#define CS_CB_DEVICE_MODE      4 //!< Режим работы устройства

#define CS_CB_UART_ZUBR_BASE   5
#define CS_CB_BAUDRATE         5 //!< Скорость обмена


//Загрузчик Flash серводвигателей в металлическом и пластиковом корпусах
#define CS_CB_START_PROG      10 //!< Запустить рабочую программу
#define CS_CB_INIT_CONFIG     11 //!< Стереть записанную конфигурацию, она будет по умолчанию
#define CS_CB_RESET_MODE      12 //!< Установить режим по умолчанию
#define CS_CB_SET_PROTOCOL    13 //!< Установить заданный протокол обмена
#define CS_CB_ERASE_PROG      14 //!< Стереть записанную программу


//Наборы для различных устройств
//Специфические параметры модуля IMU
#define CS_CB_TIME_SEC_LOW    10 //!< Секунды штампа времени
#define CS_CB_X               11
#define CS_CB_Y               12
#define CS_CB_Z               13
#define CS_CB_W               14
#define CS_CB_ACC             15


//Специфические параметры двигателя
#define CS_CB_ANGLE_BASE          10

#define CS_CB_ADC_BASE           200

#define CS_CB_TARGET_PWM         100 //Значение ШИМ
#define CS_CB_CONTROL_VALUE      500 //Значение целевого значения регулятора

#define CS_CB_RANGLE_PID_BASE     60 //Регулятор позиции
#define CS_CB_RANGLE_VELO_BASE   110 //Регулятор скорости

#define CS_CB_RLIGHT_VELO_BASE   220
#define CS_CB_RLIGHT_SPRING_BASE 240

#define CS_CB_RMOMENT_VELO_BASE  300
#define CS_CB_RMOMENT_FRIC_BASE  310

#define CS_CB_RCALIBR_VELO_BASE  350
#define CS_CB_RCALIBR_PID_BASE   370

#define CS_CB_RANGLE_T_BASE      600

#define CS_CB_RPID_PID_BASE      700
#define CS_CB_RPID_VELO_BASE     740

//Специфические параметры измерителя усилия
#define CS_CB_FORCE_TOP           10
#define CS_CB_FORCE_BOT           11
#define CS_CB_FORCE_TOP_MIN       12
#define CS_CB_FORCE_TOP_MAX       13


//Диапазон воздействий не покрывает весь диапазон значений, отправляемый в команде
// "воздействие". Поэтому мы можем специальными числами передавать в данной команде
// особые режимы работы устройств
#define CS_UM_FREE                 32767 //!< Шим от двигателя отключен
#define CS_UM_HOLD                 32766 //!< Удерживаем текущую позицию
#define CS_UM_SOFT                 32765 //!< Удерживаем текущую позицию давая возможность его изменить (пластилин)
#define CS_UM_NONE                 32764 //!< Значение не задано
#define CS_UM_SET_ZERO             32763 //!< Зафиксировать нулевую позицию
#define CS_UM_ZERO_UNLOCK          32762 //!< Разблокировать фиксацию нулевой позиции
#define CS_UM_ZP_UNLOCK            32761 //!< Разблокировать функцию настройки нулевой позиции
#define CS_UM_ZP_LOCK              32760 //!< Заблокировать функцию настройки нулевой позиции
#define CS_UM_ZP_SET_FACTORY       32759 //!< Установить заводской нулевой угол и диапазон +-150градусов
#define CS_UM_ZP_SET_NULL          32758 //!< Установка нулевого угла относительно нулевого заводского
#define CS_UM_ZP_SET_BEGIN         32757 //!< Установить значение граничного начального угла относительно нулевого
#define CS_UM_ZP_SET_END           32756 //!< Установить значение граничного конечного угла относительно нулевого



//Смещение угла для возможности регулировки в диапазоне
#define CS_ANGLE_OFFSET             1000

//Диапазон регулировки углов
#define CS_ANGLE_MIN                   0 //!< Минимальный угол регулирования 0 градусов
#define CS_ANGLE_MAX               14000 //!< Максимальный угол регулирования 307.6 градуса

//При подаче чисел в диапазоне 20000+-4000 система отправляет их прямиком на PWM
#define CS_PWM_CENTRAL             20000 //!< Нулевое значение ШИМ


inline int csMessageId( char ch ) { return ch & 0xf; }

inline int csMessageCmd( char ch ) { return (ch >> 4) & 0x7; }


class CsMessageOut
  {
    char  mBuffer[64]; //! Буфер для размещения закодированных данных
    int   mPtr;        //! Номер текущего байта
    int   mUsedBits;   //! Количество свободных битов в текущем байте
  public:
    CsMessageOut() : mPtr(0), mUsedBits(0) {}

    //!
    //! \brief addIntN Добавить N-битное значение
    //! \param val     Значение
    //! \param bits    Количество бит значения
    //!
    void addIntN( int val, int bits );

    //!
    //! \brief addInt8 Добавить 8-битное значение
    //! \param val 8-битное значение
    //!
    void addInt8( int val );

    //!
    //! \brief addInt16 Добавить 16-битное значение
    //! \param val 16-битное значение
    //!
    void addInt16( int val );

    //!
    //! \brief addInt32 Добавить 32-битное значение
    //! \param val 32-битное значение
    //!
    void addInt32( int val );

    //!
    //! \brief addFloat Добавить 32-битное с плавающей точкой
    //! \param val      32-битное с плавающей точкой
    //!
    void addFloat( float val );

    //!
    //! \brief addBlock Добавить блок байтов
    //! \param block    Блок байтов
    //! \param size     Размер блока
    //!
    void addBlock( const char *block, int size );

    //!
    //! \brief beginQuery Инициализация буфера командой cmd. После инициализации можно добавлять данные
    //! \param cmd        Формируемая команда
    //! \param id         Идентификатор устройства, которому адресована данная команда
    //!
    void beginQuery( char cmd, int id );

    //!
    //! \brief beginAnswer Инициализация буфера для ответа. После инициализации можно добавлять данные
    //!
    void beginAnswer();

    //!
    //! \brief hostBeginQuery Инициализация буфера командой cmd. После инициализации можно добавлять данные
    //! \param cmd            Формируемая команда
    //!
    void hostBeginQuery( char cmd );

    //!
    //! \brief hostBeginAnswer Инициализация буфера для ответа. После инициализации можно добавлять данные
    //!
    void hostBeginAnswer() { beginAnswer(); }

    //!
    //! \brief hostEnd Завершение формирования команды, дописывание контрольной суммы и символа \n
    //!
    void hostEnd();

    //!
    //! \brief end Завершение формирования команды и дописывание контрольной суммы
    //!
    void end();

    //!
    //! \brief lenght Возвращает текущую заполненную длину буфера
    //! \return       Длина заполненной части буфера
    //!
    int  length() const { return mPtr; }

    //!
    //! \brief buffer Возвращает буфер с сформированной строкой. Строка имеет завершающий символ \n и 0 на конце.
    //! \return Указатель на начало буфера
    //!
    const char *buffer() const { return mBuffer; }


    //==================================================================
    //  Комплексные операции
    //!
    //! \brief makeQueryControl Сформировать команду "Управление"
    //! \param id               Идентификатор устройства
    //! \param value            Значение управления
    //!
    void     makeQueryControl( int id, int value );

    //!
    //! \brief makeAnswerControl Сформировать ответ на команду "Управление"
    //! \param angle             Текущий угол сервы
    //! \param moment            Текущий момент
    //!
    void     makeAnswerControl( int angle, int moment );

    //!
    //! \brief makeQueryInfo Сформировать команду "Получить информацию"
    //! \param id            Идентификатор устройства
    //!
    void     makeQueryInfo( int id );

    //!
    //! \brief makeAnswerInfo  Сформировать ответ на команду "Получить информацию"
    //! \param val0            Значение 0
    //! \param val1            Значение 1
    //! \param val2            Значение 2
    //!
    void     makeAnswerInfo( int val0, int val1, int val2 );

    //!
    //! \brief makeQueryWrite Сформировать команду "Запись параметра"
    //! \param id             Идентификатор устройства
    //! \param index          Индекс параметра
    //! \param value          Значение параметра
    //!
    void     makeQueryWrite( int id, int index, int value );

    //!
    //! \brief makeAnswerWrite Сформировать ответ на команду "Запись параметра"
    //! \param value           Значение, записанное в параметр
    //!
    void     makeAnswerWrite( int value );

    //!
    //! \brief makeQueryRead Сформировать команду "Чтение параметра"
    //! \param id            Идентификатор устройства
    //! \param index         Индекс параметра
    //!
    void     makeQueryRead(int id, int index );

    //!
    //! \brief makeAnswerRead Сформировать ответ на команду "Чтение параметра"
    //! \param index          Индекс параметра
    //! \param value          Значение параметра
    //!
    void     makeAnswerRead( int value );

    //!
    //! \brief makeQueryFlash Сформировать команду "Прошивка"
    //! \param id             Идентификатор устройства
    //! \param adrOrCmd       Адрес прошивки или команда
    //! \param value          Значение прошивки
    //!
    void     makeQueryFlash( int id, int adrOrCmd, int value );




    //!
    //! \brief crc  Вычисление контрольной суммы для блока данных
    //! \param buf  Буфер с данными, на которых вычисляется контрольная сумма
    //! \param size Размер данных в байтах
    //! \return     Контрольная сумма
    //!
    static int  crc( const char *buf0, int size0, const char *buf1 = nullptr, int size1 = 0 );
  };


template <int len>
struct CsMessageBuf {
    const int mSize = len;  //!< Константный размер буфера
    char      mBuffer[len]; //!< Буфер, содержащий принятый блок данных
    int       mLength;      //!< Количество данных в буфере

    CsMessageBuf() : mLength(0) {}
  };


using CsMessageBuf256 = CsMessageBuf<256>;




//!
//! \brief The CsMessageIn class служит для преобразования сообщения в бинарные данные.
//! Для кодирования служит класс CsMessageOut. Кодирование и декодирование полностью согласованы,
//! поэтому нечувствительны к различиям используемых платформ.
//! Данный класс поддерживает циклический буфер.
//!
class CsMessageIn {
    const char *mBuffer;   //!< Указатель на циклический буфер
    int         mStart;    //!< Индекс начала данных в циклическом буфере
    int         mBufSize;  //!< Размер циклического буфера
    int         mPtr;      //!< Номер текущего байта декодируемого сообщения
    int         mUsedBits; //!< Количество использованных битов в текущем байте

    char  at( int index ) const { index += mStart; return mBuffer[ index < mBufSize ? index : index - mBufSize ]; }
  public:
    //!
    //! \brief SvTextStreamIn Конструктор декодера
    //! \param buf Буфер, содержащий исходную строку
    //!
    CsMessageIn( const char *buf, short start, short size = 0x7fff, int ptr = 0 );

    template <class CsMessageBufTmpl>
    CsMessageIn( CsMessageBufTmpl &buf, int ptr = 0 ) : mBuffer(buf.mBuffer), mStart(0), mBufSize(buf.mSize), mPtr(ptr), mUsedBits(0) { }

    //!
    //! \brief id Возвращает идентификатор устройства, которому адресовано сообщение
    //! \return   Идентификатор устройства
    //!
    int   id() const { return csMessageId( at(0) ); }

    //!
    //! \brief cmd Возвращает команду сообщения
    //! \return    Команда сообщения
    //!
    int   cmd() const { return csMessageCmd( at(0) ); }

    //!
    //! \brief hostCmd Возвращает команду сообщения от хоста
    //! \return        Команда сообщения
    //!
    int   hostCmd() const { return at(0); }

    //!
    //! \brief reset Установить новое начало данных и сбросить номер текущего байта и счетчик битов
    //! \param start Индекс начала новых данных
    //! \param ptr   Индекс, с которого начинаем декодирование
    //!
    void  reset( int start , int ptr );

    //!
    //! \brief getUInt8 Извлекает 8-битное число предполагая, что оно беззнаковое
    //! \return         8-битное число
    //!
    int   getUInt8();

    //!
    //! \brief getInt8 Извлекает 8-битное число
    //! \return        8-битное число со знаком
    //!
    int   getInt8();

    //!
    //! \brief getUInt16 Извлекает 16-битное число предполагая, что оно беззнаковое
    //! \return          16-битное число
    //!
    int   getUInt16();

    //!
    //! \brief getInt16 Извлекает 16-битное число со знаком
    //! \return         16-битное число со знаком
    //!
    int   getInt16();

    //!
    //! \brief getInt32 Извлекает 32-битное число
    //! \return 32-битное число
    //!
    int   getInt32();

    //!
    //! \brief getBlock Извлекает набор байтов
    //! \param dest Буфер-приемник байтов
    //! \param size Количество извлекаемых байтов
    //!
    void  getBlock( char *dest, int size );

    //!
    //! \brief checkCrc Проверить совпадение контрольной суммы
    //! \param length   Длина сообщения
    //! \return         true когда контрольная сумма совпала
    //!
    bool  checkCrc( int length ) const;
  };




//!
//! \brief floatToUInt Упаковка числа с плавающей точкой в тридцатидвухразрядную ячейку
//! \param val         Число с плавающей точкой
//! \return            Упакованное число с плавающей точкой
//!
inline uint32_t floatToUInt( float val )
  {
  // We need to do this in order to access the bits from our float
  uint32_t *u = reinterpret_cast<uint32_t*>(&val);
  return *u;
  }

//!
//! \brief floatFromUInt Распаковка из тридцатидвухразрядной ячейки в число с плавающей точкой
//! \param val           Упакованное число с плавающей точкой
//! \return              Число с плавающей точкой
//!
inline float    floatFromUInt( uint32_t val )
  {
  float *f = reinterpret_cast<float*>(&val);
  return *f;
  }


#endif // CSMESSAGE_H
