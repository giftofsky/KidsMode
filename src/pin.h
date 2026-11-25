#pragma once

#include <QByteArray>
#include <QString>

namespace Pin {

struct Hash
{
    QByteArray salt;
    QByteArray hash;
    int iterations = 100000;
};

Hash create(const QString &pin, int iterations = 100000);
bool verify(const QString &pin, const QByteArray &salt, const QByteArray &hash, int iterations);

// 0 until the 5th failure, then 5 min doubling to a 1 h cap.
int lockoutSecondsAfterFailure(int failures);

} // namespace Pin
