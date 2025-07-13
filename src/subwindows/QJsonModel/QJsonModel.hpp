/* QJsonModel.hpp
 * Copyright (c) 2011 SCHUTZ Sacha
 * Copyright © 2024 Saul D. Beniquez
 *
 * License:
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <QAbstractItemModel>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "QUtf8.hpp"
#include "../../supportFunctions.h"

class QJsonModel;
class QJsonItem;

class QJsonTreeItem : public QObject { // BG changed to qobject, so that it can participate in signals and slots
Q_OBJECT

public:
    QJsonTreeItem(QJsonTreeItem *parent = nullptr);
    ~QJsonTreeItem();
    void appendChild(QJsonTreeItem *item);
    QJsonTreeItem *child(int row);
    QJsonTreeItem *parent();
    int childCount() const;
    int row() const;
    void setKey(const QString &key);
    void setType(const QJsonValue::Type &type);
    QString key() const;
    QVariant value() const;
    QJsonValue::Type type() const;

    static QJsonTreeItem *load(const QJsonValue &value,
                             const QStringList &exceptions = {},
                             QJsonTreeItem *parent = nullptr);

    // DEV BG
    QList<QJsonTreeItem*> children() {
        QList<QJsonTreeItem*> children;
        for(int i = 0; i < childCount(); i++) {
            children.append(child(i));
        }
        return children;
    }

    // DEV BG
    QList<QJsonTreeItem*> childrenWithKey(const QString &key) {
        QList<QJsonTreeItem*> children;
        for(int i = 0; i < childCount(); i++) {
            if(child(i)->key().startsWith(key)) {
                children.append(child(i));
            }
        }
        return children;
    }

    // DEV BG
    bool hasChildWithKeyAndValue(const QString &key, const QVariant &gvalue) {
        for(int i = 0; i < childCount(); i++) {
            if(child(i)->key().startsWith(key) && child(i)->value() == gvalue) {
                return true;
            }
        }
        return false;
    }

    // DEV BG
    bool hasChildHighlighted() {
        for(int i = 0; i < childCount(); i++) {
            if(child(i)->highlightedInGUI) {
                return true;
            }
        }
        return false;
    }

    bool isHighlightedInGUI() {
        return highlightedInGUI;
    }
    void setHighlightedInGUI(bool state) {
        highlightedInGUI = state;
    }

    bool isEditable() {
        return editable;
    }

    void setEditable(bool state) {
        editable = state;
    }

    bool isEnabled() {
        return enabled;
    }

    void setEnabled(bool state) {
        enabled = state;
    }

signals:
    void valueChanged(const QVariant &value); // DEV, GB

public slots:
    void setValue(const QVariant &value); // GB: moved here

protected:
private:
    QString mKey;
    QVariant mValue;
    QJsonValue::Type mType;
    QList<QJsonTreeItem *> mChilds;
    QJsonTreeItem *mParent = nullptr;
    bool highlightedInGUI = false;
    bool editable = false;
    bool enabled = true;
};

//---------------------------------------------------

class QJsonModel : public QAbstractItemModel {
    Q_OBJECT

public:
    explicit QJsonModel(QObject *parent = nullptr);
    QJsonModel(const QString &fileName, QObject *parent = nullptr);
    QJsonModel(QIODevice *device, QObject *parent = nullptr);
    QJsonModel(const QByteArray &json, QObject *parent = nullptr);
    ~QJsonModel();
    bool load(const QString &fileName);
    bool load(QIODevice *device);
    bool loadJson(const QByteArray &json);
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QByteArray json(bool compact = false);
    QByteArray jsonToByte(QJsonValue jsonValue);
    void objectToJson(QJsonObject jsonObject, QByteArray &json, int indent,
                      bool compact);
    void arrayToJson(QJsonArray jsonArray, QByteArray &json, int indent,
                     bool compact);
    void arrayContentToJson(QJsonArray jsonArray, QByteArray &json, int indent,
                            bool compact);
    void objectContentToJson(QJsonObject jsonObject, QByteArray &json, int indent,
                             bool compact);
    void valueToJson(QJsonValue jsonValue, QByteArray &json, int indent,
                     bool compact);
      //! List of tags to skip during JSON parsing
    void addException(const QStringList &exceptions);


    /*void makeKeysFriendly() {
        for (int i = 1; i < keys.length(); i++){
            qDebug() << keys[i];
            keys[i] = SupportFunctions::camelCaseToFriendly(keys[i]);
            //      qDebug() << keys[i];
            qDebug() << "--";
        }
    }*/

    void makeKeysFriendly() {
        makeKeysFriendly(mRootItem);
        //emit beginResetModel();
    }

    // Rename the keys for better human readibility
    void makeKeysFriendly(QJsonTreeItem *item) {
        auto type = item->type();
        int nchild = item->childCount();

        QString friendlyKey;
        QString c = item->key();
        //qDebug() << "## " << c;
        QString dimensionText = "";
        if(c.contains("__")) {
            //qDebug() << c;
            if(c.length() > c.indexOf("__")+2) {
                dimensionText = c.mid(c.indexOf("__")+2, (c.length()-c.indexOf("__")-2));
                c = c.mid(0, c.length()-dimensionText.length()-2);
            } else {
                c = c.mid(0,c.length()-2);
            }
        }
        friendlyKey = SupportFunctions::camelCaseToFriendly(c);
        //
        if(dimensionText == "ma")
            dimensionText = "mA";
        if(dimensionText == "v")
            dimensionText = "V";
        if(dimensionText == "c")
            dimensionText = "°C";
        //
        if(!dimensionText.isEmpty()) {
            friendlyKey.append(" [" + dimensionText + "]");
        }

        item->setKey( friendlyKey );

        if (QJsonValue::Object == type) {
            for (int i = 0; i < nchild; ++i) {
                makeKeysFriendly(item->child(i));
            }
        }
    }

    QModelIndex indexByItem(QJsonTreeItem *item) const {
        return createIndex(item->row(), 0, item);
    };
    QModelIndex parentIndexByItem(QJsonTreeItem *item) const {
        return createIndex(item->parent()->row(), 0, item->parent());
    };

    // DEV BG
    // TODO: SZERINTEM HIBÁS, NEM A PARENTEKET ADJA VISSZA HANEM A CHILDOKAT, RÁADÁSUL MINDET
    void appendLocalChildItemsByKey(QList<QJsonTreeItem*> *candidates, const QString &key, QJsonTreeItem *node) {
        for (int j = 0; j < node->childCount(); j++) {
            if(node->child(j)->type() == QJsonValue::Object) {
                appendLocalChildItemsByKey(candidates, key, node->child(j));
            } else {
                if(node->child(j)->key().startsWith(key)) {
                    candidates->append(node->child(j));
                }
            }
        }
    }

    void appendLocalItemsByKey(QList<QJsonTreeItem*> *candidates, const QString &key, QJsonTreeItem *node) {
        for (int j = 0; j < node->childCount(); j++) {
            if(node->child(j)->type() == QJsonValue::Object) {
                appendLocalItemsByKey(candidates, key, node->child(j));
            } else {
                if(node->child(j)->key().startsWith(key)) {
                    candidates->append(node->child(j));
                }
            }
        }
    }

    void appendLocalItemsByKeyAndParentKey(QList<QJsonTreeItem*> *candidates, const QString &key, const QString &parentKey, QJsonTreeItem *node) {
        for (int j = 0; j < node->childCount(); j++) {
            if(node->child(j)->type() == QJsonValue::Object) {
                appendLocalItemsByKeyAndParentKey(candidates, key, parentKey, node->child(j));
            } else {
                if(node->child(j)->key().startsWith(key) && node->key().startsWith(parentKey)) {
                    candidates->append(node->child(j));
                }
            }
        }
    }

    void unhighlightSilently(QJsonTreeItem *node) {
        node->setHighlightedInGUI(false);
        for (int j = 0; j < node->childCount(); j++) {
            if(node->child(j)->type() == QJsonValue::Object) {
                unhighlightSilently(node->child(j));
            } else {
                node->child(j)->setHighlightedInGUI(false);
            }
        }
    }

    // DEV BG
    // NOTE: Could use regexp, but then it would be inevitably way slower,
    // as all keys would need to be retrieved first, and then searched
    // OR a grand list od keys should be maintained separately, privately,
    // and that could be searched effectively anytime.
    // TODO: fix that there may be duplicates ! (?)
    QList<QJsonTreeItem*> findItemsByChildKey(const QString key) {
        QList<QJsonTreeItem*> candidates;
        appendLocalChildItemsByKey(&candidates, key, mRootItem);
        return candidates;
    }

    QList<QJsonTreeItem*> findItemsByKey(const QString key) {
        QList<QJsonTreeItem*> candidates;
        appendLocalItemsByKey(&candidates, key, mRootItem);
        return candidates;
    }

    QList<QJsonTreeItem*> findItemsByKeyAndParentKey(const QString key, const QString parentKey) {
        QList<QJsonTreeItem*> candidates;
        appendLocalItemsByKeyAndParentKey(&candidates, key, parentKey, mRootItem);
        return candidates;
    }

    void unhighlightAllSilently() {
        unhighlightSilently(mRootItem);
    }

private:
    QJsonValue genJson(QJsonTreeItem *) const; // GB MODIFIED
    QJsonTreeItem *mRootItem = nullptr;
    QStringList mHeaders;
      //! List of exceptions (e.g. comments). Case insensitive, compairs on
      //! "contains".
    QStringList mExceptions;
};
