#include <QDebug>
#include <QIdentityProxyModel>
#include <string>
#include <vector>

#pragma once

class FileProxy : public QIdentityProxyModel
{
    Q_OBJECT
public:
    FileProxy() {}

    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override { return QModelIndex{}; }

    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override
    {
        return QModelIndex{};
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const
    {
        // if (parent.isValid())
        // {
        //     return QIdentityProxyModel::rowCount(parent);
        // }
        // else
        // {
        //     return QIdentityProxyModel::rowCount(parent) + 1;
        // }
        return files.size();
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        int row = index.row();
        qDebug() << "index row is " << row;

        return QString::fromStdString(files[row]);
    }

private:
    std::vector<std::string> files{ "aaa", "bbb", "ccc" };
};