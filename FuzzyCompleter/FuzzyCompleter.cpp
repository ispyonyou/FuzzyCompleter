#include "FuzzyCompleter.h"
#include <QSortFilterProxyModel>
#include <QAbstractItemView>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QTextDocument>
#include <QTextCursor>

class FuzzyCompleterDelegate : public QStyledItemDelegate
{
public:
    explicit FuzzyCompleterDelegate( FuzzyCompleter* completer );

    virtual void paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const override;
    QSize sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const;

    void initTextDocument( QTextDocument& doc, const QModelIndex &index ) const;

private:
    FuzzyCompleter* completer;
};

///////////////////////////////////////////////////////////////////////////////
class FuzzyCompleterProxyModel : public QSortFilterProxyModel
{
public:
    explicit FuzzyCompleterProxyModel(FuzzyCompleter* completer, QObject *parent = 0);

    bool isMatched( const QString& s, QList< long >& matchedPositions ) const;

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;
    virtual bool lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const override;

private:
    FuzzyCompleter* completer;
};

///////////////////////////////////////////////////////////////////////////////
class FuzzyCompleterPrivate
{
public:
    QString local_completion_prefix;
    std::map< long, QList< long > > matched_indexes;
    std::map< long, long > first_match;
    std::map< long, long > maximum_sequence;
    QAbstractItemModel* source_model;
};

///////////////////////////////////////////////////////////////////////////////
FuzzyCompleter::FuzzyCompleter( QObject *parent )
    : QCompleter( parent )
    , d( *new FuzzyCompleterPrivate )
{
    d.source_model = nullptr;
    popup()->setItemDelegateForColumn( 0, new FuzzyCompleterDelegate( this ) );
}

FuzzyCompleter::~FuzzyCompleter()
{
  delete &d;
}

void FuzzyCompleter::setModel(QAbstractItemModel *c)
{
    d.source_model = c;
    QCompleter::setModel( d.source_model );
}

void FuzzyCompleter::updateModel()
{
    FuzzyCompleterProxyModel* proxy_model = new FuzzyCompleterProxyModel( this );
    proxy_model->setSourceModel( d.source_model );
    QCompleter::setModel( proxy_model );
    proxy_model->sort( 0 );
}

QStringList FuzzyCompleter::splitPath(const QString &path) const
{
    FuzzyCompleter* that = const_cast< FuzzyCompleter* >( this );

    that->d.local_completion_prefix = path;
    that->updateModel();

    return QStringList();
}

///////////////////////////////////////////////////////////////////////////////
FuzzyCompleterDelegate::FuzzyCompleterDelegate( FuzzyCompleter* completer )
    : QStyledItemDelegate( completer )
{
    this->completer = completer;
}

void FuzzyCompleterDelegate::paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption( &options, index );

    painter->save();

    QTextDocument doc;
    initTextDocument( doc, index );

    options.text = "";
    options.widget->style()->drawControl( QStyle::CE_ItemViewItem, &options, painter );

    painter->translate( options.rect.left(), options.rect.top() );
    QRect clip( 0, 0, options.rect.width(), options.rect.height() );
    doc.drawContents( painter, clip );

    painter->restore();
}

QSize FuzzyCompleterDelegate::sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
//      QStyleOptionViewItemV4 options = option;
//      initStyleOption(&options, index);
//
//      QTextDocument doc;
//      initTextDocument( doc, index );
//      doc.setTextWidth(options.rect.width());
//
//      return QSize(doc.idealWidth(), doc.size().height());

    return QStyledItemDelegate::sizeHint( option, index );
}

void FuzzyCompleterDelegate::initTextDocument( QTextDocument& doc, const QModelIndex &index ) const
{
    QList< long > matchedPositions;

    // QCompletionModel
    const QAbstractProxyModel* completionModel = dynamic_cast< const QAbstractProxyModel* >( index.model() );
    if( completionModel )
    {
        const QAbstractItemModel* completionSourceModel = completionModel->sourceModel();

        const FuzzyCompleterProxyModel* fuzzyProxyModel = dynamic_cast< const FuzzyCompleterProxyModel* >( completionSourceModel );
        if( fuzzyProxyModel )
        {
            QModelIndex fuzzyProxyModelIndex = completionModel->mapToSource( index );
            QModelIndex sourceModelIndex = fuzzyProxyModel->mapToSource( fuzzyProxyModelIndex );

            matchedPositions = completer->d.matched_indexes[ sourceModelIndex.row() ];
        }
    }

    QString text = index.model()->data( index, Qt::DisplayRole ).toString();

    doc.setPlainText( text );

    QTextCursor cursor( &doc );

    for( long indexPosition = 0; indexPosition <  matchedPositions.length(); indexPosition++ )
    {
        cursor.setPosition( matchedPositions[ indexPosition ] );
        cursor.movePosition( QTextCursor::Right, QTextCursor::KeepAnchor );

        QTextCharFormat format;
//        format.setFontWeight( QFont::Bold );
        format.setForeground( QBrush( Qt::red ) );

        cursor.mergeCharFormat( format );
    }
}

///////////////////////////////////////////////////////////////////////////////
FuzzyCompleterProxyModel::FuzzyCompleterProxyModel(FuzzyCompleter* completer, QObject *parent )
    : QSortFilterProxyModel(parent)
{
    this->completer = completer;
}

bool FuzzyCompleterProxyModel::isMatched( const QString& s, QList< long >& matchedPositions ) const
{
    long tokenIndex = 0;

    for( long stringIndex = 0; stringIndex < s.length(); stringIndex++ )
    {
        if( s[ stringIndex ] == completer->d.local_completion_prefix[ tokenIndex ] )
        {
            matchedPositions.push_back( stringIndex );
            tokenIndex++;

            if( tokenIndex >= completer->d.local_completion_prefix.length() )
                return true;
        }
    }

    return false;
}

bool FuzzyCompleterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex index0 = sourceModel()->index(source_row, 0, source_parent);
    QString text = sourceModel()->data( index0 ).toString().toLower();

    QList< long > matchedPositions;
    if( !isMatched( text, matchedPositions ) )
        return false;

    completer->d.matched_indexes[ index0.row() ] = matchedPositions;
    completer->d.first_match[ index0.row() ] = matchedPositions[ 0 ];

    auto calc_max_seq = [ &matchedPositions ]() -> long {
        long max_seq = 1;
        long cur_max_seq = 1;
        for( long i = 1; i < matchedPositions.length(); i++ )
        {
            if( matchedPositions[ i - 1 ] == matchedPositions[ i ] - 1 )
            {
                cur_max_seq++;
            }
            else
            {
                if( max_seq < cur_max_seq )
                    max_seq = cur_max_seq;
                cur_max_seq = 1;
            }
        }

        if( max_seq < cur_max_seq )
            max_seq = cur_max_seq;

        return max_seq;
    };

    completer->d.maximum_sequence[ index0.row() ] = calc_max_seq();

    return true;
}

bool FuzzyCompleterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
//    QModelIndex source_index_left = sourceModel()->index(source_left, 0, QModelIndex());
//    QModelIndex source_index_right = sourceModel()->index(source_right, 0, QModelIndex());

    long max_seq_left = completer->d.maximum_sequence[ source_left.row() ];
    long max_seq_right = completer->d.maximum_sequence[ source_right.row() ];

    if( max_seq_left != max_seq_right )
        return max_seq_left > max_seq_right;

    long first_match_left = completer->d.first_match[ source_left.row() ];
    long first_match_right = completer->d.first_match[ source_right.row() ];

    if( first_match_left != first_match_right )
        return first_match_left < first_match_right;

    return QSortFilterProxyModel::lessThan( source_left, source_right );
}
