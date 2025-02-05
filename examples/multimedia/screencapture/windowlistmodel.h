// Copyright (C) 2022 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef WINDOWLISTMODEL_H
#define WINDOWLISTMODEL_H

#include <QAbstractListModel>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

QT_USE_NAMESPACE

class WindowListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit WindowListModel(QList<QWindow *> data = QList<QWindow *>(), QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QWindow *window(const QModelIndex &index) const;

private:
    QList<QWindow *> windowList;
};

#endif // WINDOWLISTMODEL_H
