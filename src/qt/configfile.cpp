#include "configfile.h"

#include <QStringList>
#include <QTextStream>

ConfigFile::ConfigFile()
{
    mFile = new QFile("config.plc");
    clear();
    load();
}

ConfigFile::~ConfigFile()
{
    save();
    delete mFile;
}

QString ConfigFile::getData() const
{
    return mData;
}

bool ConfigFile::load()
{
    if(!mFile->open(QIODevice::ReadWrite | QIODevice::Text))
    {
        mMaxHashes = 10000;
        return false;
    }

    QTextStream in(mFile);
    mData = in.readLine();

    QStringList list = mData.split(";");
    if(list.size() > 0)
    {
        mMaxHashes = list[0].toInt();
    }

    return true;
}

bool ConfigFile::save()
{
    mFile->reset();
    QTextStream out(mFile);
    out << mMaxHashes << ";";
    mFile->close();
    return true;
}

void ConfigFile::clear()
{
    mData.clear();
    mMaxHashes = 0;
}

void ConfigFile::setMaxHashes(int inMaxHashes)
{
    if(inMaxHashes > mMaxHashes) {
        mMaxHashes = inMaxHashes;
    }
}

int ConfigFile::getMaxHashes()
{
    return mMaxHashes;
}
