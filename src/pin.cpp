#include "pin.h"

#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QtNetwork/QPasswordDigestor>

namespace {

bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size())
        return false;

    unsigned char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= static_cast<unsigned char>(a.at(i) ^ b.at(i));
    return diff == 0;
}

} // namespace

namespace Pin {

Hash create(const QString &pin, int iterations)
{
    Hash result;
    result.iterations = qMax(1000, iterations);

    quint32 words[4];
    QRandomGenerator::system()->fillRange(words);
    result.salt = QByteArray(reinterpret_cast<const char *>(words), sizeof(words));

    result.hash = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                                     pin.toUtf8(),
                                                     result.salt,
                                                     result.iterations,
                                                     32);
    return result;
}

bool verify(const QString &pin, const QByteArray &salt, const QByteArray &hash, int iterations)
{
    if (salt.isEmpty() || hash.isEmpty() || iterations <= 0)
        return false;

    const QByteArray derived = QPasswordDigestor::deriveKeyPbkdf2(QCryptographicHash::Sha256,
                                                                 pin.toUtf8(),
                                                                 salt,
                                                                 iterations,
                                                                 hash.size());
    return constantTimeEquals(derived, hash);
}

int lockoutSecondsAfterFailure(int failures)
{
    if (failures < 5)
        return 0;

    const int step = qMin(failures - 5, 5);
    const qint64 seconds = 300LL << step;
    return static_cast<int>(qMin<qint64>(seconds, 3600));
}

} // namespace Pin
