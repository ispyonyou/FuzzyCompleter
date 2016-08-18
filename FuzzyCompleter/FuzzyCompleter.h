#ifndef FUZZYCOMPLETER_H
#define FUZZYCOMPLETER_H

#include <QCompleter>

class FuzzyCompleterPrivate;

class FuzzyCompleter : public QCompleter
{
    friend class FuzzyCompleterProxyModel;
    friend class FuzzyCompleterDelegate;

public:
    explicit FuzzyCompleter(QObject *parent = 0);
    ~FuzzyCompleter();

    void setModel(QAbstractItemModel *c);

    void updateModel();

    virtual QStringList splitPath(const QString &path) const override;

private:
    FuzzyCompleterPrivate& d;
};

#endif // FUZZYCOMPLETER_H
