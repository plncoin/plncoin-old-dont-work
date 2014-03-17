#ifndef FILES_H
#define FILES_H

#include <QFile>
#include <QString>

class ConfigFile
{
public:

    ConfigFile();
    virtual ~ConfigFile();

    QString getData() const;

    void setMaxHashes(int inMaxHashes);
    int getMaxHashes();

private:
    bool load();
    bool save();
    void clear();

    QFile *mFile;
    QString mData;
    int mMaxHashes;
};

#endif // FILES_H
