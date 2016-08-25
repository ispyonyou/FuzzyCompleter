#include <QApplication>
#include <QWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QStringListModel>
#include "FuzzyCompleter.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QLineEdit* le = new QLineEdit();
    {
        FuzzyCompleter* completer = new FuzzyCompleter( le );
        {
            QStringList completerItems;
            completerItems << "0123" << "1234" << "2345" << "3456" << "4567" << "5678" << "6789" << "7890" << "8901" << "9012"
                           << "0246" << "2468" << "4680" << "6802" << "8024" << "1357" << "3579" << "5791" << "7913" << "9135";

            completer->setModel( new QStringListModel( completerItems, le ) );
        }


        le->setCompleter( completer );
    }

    QVBoxLayout* l = new QVBoxLayout();

    l->addWidget( le );
    l->addStretch();

    QWidget w;
    w.setLayout( l );

    w.show();

    return a.exec();
}
