/****************************************************************************
** Meta object code from reading C++ file 'EngineWorker.hpp'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../src/gui/EngineWorker.hpp"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'EngineWorker.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN12EngineWorkerE_t {};
} // unnamed namespace

template <> constexpr inline auto EngineWorker::qt_create_metaobjectdata<qt_meta_tag_ZN12EngineWorkerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "EngineWorker",
        "statusChanged",
        "",
        "status",
        "progressChanged",
        "percent",
        "elapsedMs",
        "etaMs",
        "previewReady",
        "QImage",
        "preview",
        "finished",
        "finalImage",
        "cancelled",
        "failed",
        "message",
        "run"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'progressChanged'
        QtMocHelpers::SignalData<void(int, qint64, qint64)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 5 }, { QMetaType::LongLong, 6 }, { QMetaType::LongLong, 7 },
        }}),
        // Signal 'previewReady'
        QtMocHelpers::SignalData<void(const QImage &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void(const QImage &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 12 },
        }}),
        // Signal 'cancelled'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'failed'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'run'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<EngineWorker, qt_meta_tag_ZN12EngineWorkerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject EngineWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12EngineWorkerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12EngineWorkerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN12EngineWorkerE_t>.metaTypes,
    nullptr
} };

void EngineWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<EngineWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->statusChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->progressChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[3]))); break;
        case 2: _t->previewReady((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1]))); break;
        case 3: _t->finished((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1]))); break;
        case 4: _t->cancelled(); break;
        case 5: _t->failed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 6: _t->run(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (EngineWorker::*)(const QString & )>(_a, &EngineWorker::statusChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineWorker::*)(int , qint64 , qint64 )>(_a, &EngineWorker::progressChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineWorker::*)(const QImage & )>(_a, &EngineWorker::previewReady, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineWorker::*)(const QImage & )>(_a, &EngineWorker::finished, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineWorker::*)()>(_a, &EngineWorker::cancelled, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (EngineWorker::*)(const QString & )>(_a, &EngineWorker::failed, 5))
            return;
    }
}

const QMetaObject *EngineWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *EngineWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN12EngineWorkerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int EngineWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void EngineWorker::statusChanged(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void EngineWorker::progressChanged(int _t1, qint64 _t2, qint64 _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2, _t3);
}

// SIGNAL 2
void EngineWorker::previewReady(const QImage & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void EngineWorker::finished(const QImage & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void EngineWorker::cancelled()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void EngineWorker::failed(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}
QT_WARNING_POP
