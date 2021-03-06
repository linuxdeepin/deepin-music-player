/*
 * Copyright (C) 2016 ~ 2018 Wuhan Deepin Technology Co., Ltd.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "playlistview.h"

#include <QDebug>
#include <DMenu>
#include <QDir>
#include <QProcess>
#include <QFileInfo>
#include <QStyleFactory>
#include <QResizeEvent>
#include <QStandardItemModel>
#include <QVariant>
#include <QShortcut>
#include <QMimeData>

#include <DDialog>
#include <DDesktopServices>
#include <DScrollBar>
#include <DGuiApplicationHelper>
#include <DLabel>
#include "util/pinyinsearch.h"

#include "playitemdelegate.h"
#include "playlistmodel.h"
#include "databaseservice.h"
#include "global.h"
#include "mediameta.h"
#include "player.h"
#include "commonservice.h"
#include "util/global.h"
#include "metadetector.h"
#include "infodialog.h"
#include "speechCenter.h"
#include "ac-desktop-define.h"

DWIDGET_USE_NAMESPACE
// 升序
bool moreThanTimestampASC(MediaMeta v1, MediaMeta v2)
{
    return v1.timestamp < v2.timestamp;
}

bool moreThanTitleASC(const MediaMeta v1, const MediaMeta v2)
{
    return v1.pinyinTitle < v2.pinyinTitle;
}

bool moreThanSingerASC(MediaMeta v1, MediaMeta v2)
{
    return v1.pinyinArtist < v2.pinyinArtist;
}

bool moreThanAblumASC(const MediaMeta v1, const MediaMeta v2)
{
    return v1.pinyinAlbum < v2.pinyinAlbum;
}
// 降序
bool moreThanTimestampDES(MediaMeta v1, MediaMeta v2)
{
    return v1.timestamp > v2.timestamp;
}

bool moreThanTitleDES(const MediaMeta v1, const MediaMeta v2)
{
    return v1.pinyinTitle > v2.pinyinTitle;
}

bool moreThanSingerDES(MediaMeta v1, MediaMeta v2)
{
    return v1.pinyinArtist > v2.pinyinArtist;
}

bool moreThanAblumDES(const MediaMeta v1, const MediaMeta v2)
{
    return v1.pinyinAlbum > v2.pinyinAlbum;
}

// 排序
void PlayListView::sortList(QList<MediaMeta> &musicInfos, const DataBaseService::ListSortType &sortType)
{
    switch (sortType) {
    case DataBaseService::SortByAddTimeASC: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanTimestampASC);
        break;
    }
    case DataBaseService::SortByTitleASC: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanTitleASC);
        break;
    }
    case DataBaseService::SortBySingerASC: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanSingerASC);
        break;
    }
    case DataBaseService::SortByAblumASC: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanAblumASC);
        break;
    }
    case DataBaseService::SortByAddTimeDES: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanTimestampDES);
        break;
    }
    case DataBaseService::SortByTitleDES: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanTitleDES);
        break;
    }
    case DataBaseService::SortBySingerDES: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanSingerDES);
        break;
    }
    case DataBaseService::SortByAblumDES: {
        qSort(musicInfos.begin(), musicInfos.end(), moreThanAblumDES);
        break;
    }
    default:
        break;
    }
}

PlayListView::PlayListView(QString hash, bool isPlayQueue, QWidget *parent)
    : DListView(parent)
    , m_currentHash(hash.isEmpty() ? "all" : hash)
{
    m_IsPlayQueue = isPlayQueue;
    setObjectName("PlayListView");

    m_model = new PlaylistModel(0, 1, this);
    m_model->clear();
    setModel(m_model);

    m_delegate = new PlayItemDelegate(this);
    setItemDelegate(m_delegate);

    setUniformItemSizes(true);

    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragEnabled(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::DragDrop);
    setDefaultDropAction(Qt::MoveAction);

    setMovement(QListView::Free);
    //默认QListView::ListMode
    setViewModeFlag(m_currentHash, QListView::ListMode);

    setResizeMode(QListView::Adjust);
    setLayoutMode(QListView::Batched);
    setBatchSize(2000);

    setSelectionMode(QListView::ExtendedSelection);
//setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    setContextMenuPolicy(Qt::DefaultContextMenu);

    //detail shortcut
    m_pDetailShortcut = new QShortcut(this);
    m_pDetailShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    m_pDetailShortcut->setKey(QKeySequence(QLatin1String("Ctrl+I")));
    connect(m_pDetailShortcut, SIGNAL(activated()), this, SLOT(showDetailInfoDlg()));
//    //快捷移出歌单
//    m_pRmvSongsShortcut = new QShortcut(this);
//    m_pRmvSongsShortcut->setKey(QKeySequence(QLatin1String("Delete")));
//    connect(m_pRmvSongsShortcut, SIGNAL(activated()), this, SLOT(slotRmvFromSongList()));
//    //快捷显示菜单
//    m_pShowMenuShortcut = new QShortcut(this);
//    m_pShowMenuShortcut->setKey(QKeySequence(QLatin1String("Alt+M")));

    connect(this, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(slotOnDoubleClicked(const QModelIndex &)));

    connect(Player::getInstance(), &Player::signalUpdatePlayingIcon,
            this, &PlayListView::slotUpdatePlayingIcon);
    connect(Player::getInstance(), &Player::signalPlayQueueMetaRemove,
            this, &PlayListView::slotPlayQueueMetaRemove);

    connect(DataBaseService::getInstance(), &DataBaseService::signalCoverUpdate,
            this, &PlayListView::slotCoverUpdate);

    connect(DataBaseService::getInstance(), &DataBaseService::signalRmvSong,
            this, &PlayListView::slotRemoveSingleSong);

    connect(DataBaseService::getInstance(), &DataBaseService::signalMusicAddOne,
            this, &PlayListView::slotMusicAddOne);

    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, &PlayListView::setThemeType);

    setThemeType(DGuiApplicationHelper::instance()->themeType());
    // 跳转到播放的位置
    connect(CommonService::getInstance(), &CommonService::sigScrollToCurrentPosition,
            this, &PlayListView::slotScrollToCurrentPosition);
    // 刷新当前页面编码
    connect(CommonService::getInstance(), &CommonService::signalUpdateCodec,
            this, &PlayListView::slotUpdateCodec);
    // 移出cda格式歌曲
    connect(CommonService::getInstance(), &CommonService::signalCdaSongListChanged,
            this, &PlayListView::rmvCdaSongs);
}

PlayListView::~PlayListView()
{

}

void PlayListView::setThemeType(int type)
{
    m_themeType = type;
}

int PlayListView::getThemeType() const
{
    return m_themeType;
}

QStandardItem *PlayListView::item(int row, int column) const
{
    return  m_model->item(row, column);
}

//void PlayListView::setCurrentItem(QStandardItem *item)
//{
//    setCurrentIndex(m_model->indexFromItem(item));
//}

void PlayListView::initAllSonglist(QString hash)
{
    m_currentHash = hash;
    m_model->clear();
    QList<MediaMeta> mediaMetas = DataBaseService::getInstance()->allMusicInfos();

    DataBaseService::ListSortType sortType = getSortType();
    this->setDataBySortType(mediaMetas, sortType);
}

void PlayListView::initCostomSonglist(const QString &hash)
{
    if (DataBaseService::getInstance()->getDelStatu()) {
        DataBaseService::getInstance()->setDelNeedSleep();
    }

    m_currentHash = hash;

    QList<MediaMeta> mediaMetas;
    DataBaseService::ListSortType sortType = DataBaseService::SortByNull;
    if (hash == "CdaRole") { //从player获取cda缓存数据
        mediaMetas = Player::getInstance()->getCdaPlayList();
    } else {
        mediaMetas = DataBaseService::getInstance()->customizeMusicInfos(hash);
        sortType = getSortType();
    }

    this->setDataBySortType(mediaMetas, sortType);
}

void PlayListView::resetSonglistByStr(const QString &searchWord)
{
    m_model->clear();
    QList<MediaMeta> mediaMetas = DataBaseService::getInstance()->allMusicInfos();

    DataBaseService::ListSortType sortType = getSortType();

    // 排序
    sortList(mediaMetas, sortType);

    for (int i = 0; i < mediaMetas.size(); i++) {
        if (!CommonService::getInstance()->containsStr(searchWord, mediaMetas.at(i).title)) {
            continue;
        }
        QStandardItem *newItem = new QStandardItem;

        QString imagesDirPath = Global::cacheDir() + "/images/" + mediaMetas.at(i).hash + ".jpg";
        QFileInfo file(imagesDirPath);
        QIcon icon;
        if (file.exists()) {
            icon = QIcon(imagesDirPath);
        } else {
            icon = QIcon::fromTheme("cover_max");
        }
        newItem->setIcon(icon);
        m_model->appendRow(newItem);

        auto row = m_model->rowCount() - 1;
        QModelIndex index = m_model->index(row, 0, QModelIndex());

        QVariant mediaMeta;//
        MediaMeta meta = mediaMetas.at(i);
        mediaMeta.setValue(meta);
        m_model->setData(index, mediaMeta, Qt::UserRole);
    }
}

void PlayListView::resetSonglistByAlbum(const QList<AlbumInfo> &albuminfos)
{
    QList<MediaMeta> mediaMetas;
    for (AlbumInfo albuminfo : albuminfos) {
        for (MediaMeta meta : albuminfo.musicinfos.values()) {
            mediaMetas.append(meta);
        }
    }
    DataBaseService::ListSortType sortType = getSortType();
    this->setDataBySortType(mediaMetas, sortType);
}

void PlayListView::resetSonglistBySinger(const QList<SingerInfo> &singerInfos)
{
    QList<MediaMeta> mediaMetas;
    for (SingerInfo singerInfo : singerInfos) {
        for (MediaMeta meta : singerInfo.musicinfos.values()) {
            mediaMetas.append(meta);
        }
    }
    DataBaseService::ListSortType sortType = getSortType();
    this->setDataBySortType(mediaMetas, sortType);
}

QList<MediaMeta> PlayListView::getMusicListData()
{
    QList<MediaMeta> list;
    for (int i = 0; i < m_model->rowCount(); i++) {
        QModelIndex idx = m_model->index(i, 0, QModelIndex());
        MediaMeta meta = idx.data(Qt::UserRole).value<MediaMeta>();
        list.append(meta);
    }
    return list;
}

void PlayListView::setDataBySortType(QList<MediaMeta> &mediaMetas, DataBaseService::ListSortType sortType)
{
    // 排序
    sortList(mediaMetas, sortType);
    m_model->clear();
    for (int i = 0; i < mediaMetas.size(); i++) {
        QStandardItem *newItem = new QStandardItem;

        QString imagesDirPath = Global::cacheDir() + "/images/" + mediaMetas.at(i).hash + ".jpg";
        QFileInfo file(imagesDirPath);
        QIcon icon;
        if (file.exists()) {
            icon = QIcon(imagesDirPath);
        } else {
            icon = QIcon::fromTheme("cover_max");
        }
        newItem->setIcon(icon);
        m_model->appendRow(newItem);

        auto row = m_model->rowCount() - 1;
        QModelIndex index = m_model->index(row, 0, QModelIndex());

        QVariant mediaMeta;
        MediaMeta meta = mediaMetas.at(i);
        mediaMeta.setValue(meta);
        m_model->setData(index, mediaMeta, Qt::UserRole);
    }
}

QPixmap PlayListView::getPlayPixmap(bool isSelect)
{
    QColor color;
    if (isSelect) {
        color = QColor(Qt::white);
    } else {
        color = QColor(DGuiApplicationHelper::instance()->applicationPalette().highlight().color());
    }

    QImage playingImage = Player::getInstance()->playingIcon().pixmap(QSize(20, 20), QIcon::Active, QIcon::On).toImage();
    for (int i = 0; i < playingImage.width(); i++) {
        for (int j = 0; j < playingImage.height(); j++) {
            if (playingImage.pixelColor(i, j) != QColor(0, 0, 0, 0)) {
                playingImage.setPixelColor(i, j, color);
            }
        }
    }
    QPixmap playingPixmap = QPixmap::fromImage(playingImage);
    update();
    return playingPixmap;
}

void PlayListView::playListChange()
{
    //todo..
    setUpdatesEnabled(false);
    setAutoScroll(false);

    m_model->clear();
    for (auto meta : *Player::getInstance()->getPlayList()) {
        QStandardItem *newItem = new QStandardItem;
        m_model->appendRow(newItem);
        auto row = m_model->rowCount() - 1;
        QModelIndex index = m_model->index(row, 0, QModelIndex());
        QVariant mediaMeta;
        mediaMeta.setValue(meta);
        m_model->setData(index, mediaMeta, Qt::UserRole);
    }
    setAutoScroll(true);
    setUpdatesEnabled(true);
    update();
}

//void PlayListView::setCurrentHash(QString hash)
//{
//    m_currentHash = hash;
//}

QString PlayListView::getCurrentHash()
{
    return m_currentHash;
}

QListView::ViewMode PlayListView::getViewMode()
{
    if (!m_viewModeMap.contains(m_currentHash)) {
        m_viewModeMap[m_currentHash] = QListView::ViewMode::ListMode;
    }
    return m_viewModeMap[m_currentHash];
}

DataBaseService::ListSortType PlayListView::getSortType()
{
    DataBaseService::ListSortType type = static_cast<DataBaseService::ListSortType>
                                         (DataBaseService::getInstance()->getPlaylistSortType(m_currentHash));
    return type;
}

void PlayListView::setSortType(DataBaseService::ListSortType sortType)
{
    // 倒序
    switch (sortType) {
    case DataBaseService::SortByAddTime: {
        if (getSortType() == DataBaseService::SortByAddTimeASC) {
            sortType = DataBaseService::SortByAddTimeDES;
        } else {
            sortType = DataBaseService::SortByAddTimeASC;
        }
        break;
    }
    case DataBaseService::SortByTitle: {
        if (getSortType() == DataBaseService::SortByTitleASC) {
            sortType = DataBaseService::SortByTitleDES;
        } else {
            sortType = DataBaseService::SortByTitleASC;
        }
        break;
    }
    case DataBaseService::SortBySinger: {
        if (getSortType() == DataBaseService::SortBySingerASC) {
            sortType = DataBaseService::SortBySingerDES;
        } else {
            sortType = DataBaseService::SortBySingerASC;
        }
        break;
    }
    case DataBaseService::SortByAblum: {
        if (getSortType() == DataBaseService::SortByAblumASC) {
            sortType = DataBaseService::SortByAblumDES;
        } else {
            sortType = DataBaseService::SortByAblumASC;
        }
        break;
    }
    default:
        sortType = DataBaseService::SortByAddTimeASC;
        break;
    }

    DataBaseService::getInstance()->updatePlaylistSortType(sortType, m_currentHash);
    QList<MediaMeta> mediaMetas = getMusicListData();
    this->setDataBySortType(mediaMetas, sortType);
}

void PlayListView::showErrorDlg()
{
    QList<DDialog *> ql = this->findChildren<DDialog *>("uniqueinvaliddailog");
    if (ql.size() > 0) {
        if (!ql.first()->isHidden())
            return ;
    }

    Dtk::Widget::DDialog warnDlg(this);
    warnDlg.setObjectName("uniqueinvaliddailog");
    warnDlg.setIcon(QIcon::fromTheme("deepin-music"));
    warnDlg.setTextFormat(Qt::RichText);
    warnDlg.setTitle(tr("File is invalid or does not exist, load failed"));
    warnDlg.addButtons(QStringList() << tr("OK"));
    warnDlg.setDefaultButton(0);
    if (0 == warnDlg.exec()) {
        //播放下一首
        Player::getInstance()->playNextMeta(true);
    }
}

int PlayListView::getMusicCount()
{
    return m_model->rowCount();
}

void PlayListView::setMusicListView(QMap<QString, MediaMeta> musicinfos, QString hash)
{
    m_currentHash = hash;
    m_model->clear();
    for (int i = 0; i < musicinfos.values().size(); i++) {
        QStandardItem *newItem = new QStandardItem;

        QString imagesDirPath = Global::cacheDir() + "/images/" + musicinfos.values().at(i).hash + ".jpg";
        QFileInfo file(imagesDirPath);
        QIcon icon;
        if (file.exists()) {
            icon = QIcon(imagesDirPath);
        } else {
            icon = QIcon::fromTheme("cover_max");
        }
        newItem->setIcon(icon);
        m_model->appendRow(newItem);

        auto row = m_model->rowCount() - 1;
        QModelIndex index = m_model->index(row, 0, QModelIndex());

        QVariant mediaMeta;
        MediaMeta meta = musicinfos.values().at(i);
        mediaMeta.setValue(meta);
        m_model->setData(index, mediaMeta, Qt::UserRole);
    }
}

void PlayListView::setViewModeFlag(QString hash, QListView::ViewMode mode)
{
    m_viewModeMap[hash] = mode;
    if (mode == QListView::IconMode) {
        setIconSize(QSize(150, 150));
        setGridSize(QSize(-1, -1));
        setSpacing(20);
        // 修改底部间距
        setViewportMargins(-10, -13, -35, 0);
    } else {
        setIconSize(QSize(36, 36));
        setGridSize(QSize(-1, -1));
        setSpacing(0);
        // 修改顶部间距
        setViewportMargins(0, 5, 8, 0);
    }
    setViewMode(mode);
}

void PlayListView::slotOnDoubleClicked(const QModelIndex &index)
{
    //todo检查文件是否存在
    MediaMeta itemMeta = index.data(Qt::UserRole).value<MediaMeta>();
    qDebug() << "------" << itemMeta.hash;
    if (!QFileInfo(itemMeta.localPath).exists() && itemMeta.mmType != MIMETYPE_CDA) {
        //停止当前的歌曲
        Player::getInstance()->stop();
        //弹出提示框
        showErrorDlg();
    } else {
        playMusic(itemMeta);
    }
}

void PlayListView::slotUpdatePlayingIcon()
{
    this->update();
}

void PlayListView::slotCoverUpdate(const MediaMeta &meta)
{
    for (int i = 0; i < m_model->rowCount(); i++) {
        QModelIndex idx = m_model->index(i, 0, QModelIndex());
        MediaMeta metaBind = idx.data(Qt::UserRole).value<MediaMeta>();

        if (metaBind.hash == meta.hash) {
            QStandardItem *item = dynamic_cast<QStandardItem *>(m_model->item(i, 0));
            if (item == nullptr) {
                break;
            }
            QString imagesDirPath = Global::cacheDir() + "/images/" + meta.hash + ".jpg";
            QFileInfo file(imagesDirPath);
            QIcon icon;
            if (file.exists()) {
                item->setIcon(QIcon(imagesDirPath));
            } else {
                item->setIcon(m_defaultIcon);
            }
            break;
        }
    }
}

void PlayListView::slotTheme(int type)
{
    m_themeType = type;
}

void PlayListView::slotRemoveSingleSong(const QString &listHash, const QString &musicHash)
{
    if (listHash == "all" || listHash == m_currentHash) {
        int row =  m_model->rowCount();
        for (int i = 0; i < row; i++) {
            QModelIndex mindex = m_model->index(i, 0, QModelIndex());
            MediaMeta meta = mindex.data(Qt::UserRole).value<MediaMeta>();
            if (meta.hash == musicHash) {
                m_model->removeRow(mindex.row());
                //搜索结果中删除，刷新数量显示
                if (m_currentHash == "musicResult") {
                    emit musicResultListCountChanged("musicResult");
                }
                break;
            }
        }
    }
    if (m_model->rowCount() == 0) {
        emit CommonService::getInstance()->signalHideSubSonglist();
    }
}

void PlayListView::slotMusicAddOne(QString listHash, MediaMeta addMeta)
{
    if (!this->isVisible() || listHash != m_currentHash) {
        return;
    }
    DataBaseService::ListSortType sortType = DataBaseService::SortByAddTimeASC;//getSortType();
    if (m_model->rowCount() == 0) {
        insertRow(0, addMeta);
    } else {
        bool isInserted = false;
        //如果已经存在，则不加入
        if (!isContain(addMeta.hash)) {
            for (int rowIndex = 0; rowIndex < m_model->rowCount(); rowIndex++) {
                isInserted = false;
                QModelIndex index = m_model->index(rowIndex, 0, QModelIndex());
                MediaMeta meta = index.data(Qt::UserRole).value<MediaMeta>();
                switch (sortType) {
                case DataBaseService::SortByAddTimeASC: {
                    if (addMeta.timestamp <= meta.timestamp) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                case DataBaseService::SortByTitleASC: {
                    if (addMeta.pinyinTitle <= meta.pinyinTitle) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                case DataBaseService::SortBySingerASC: {
                    if (addMeta.pinyinArtist <= meta.pinyinArtist) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                case DataBaseService::SortByAblumASC: {
                    if (addMeta.pinyinAlbum <= meta.pinyinAlbum) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                case DataBaseService::SortByAddTimeDES: {
                    if (addMeta.timestamp >= meta.timestamp) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                case DataBaseService::SortByTitleDES: {
                    if (addMeta.pinyinTitle >= meta.pinyinTitle) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                case DataBaseService::SortBySingerDES: {
                    if (addMeta.pinyinArtist >= meta.pinyinArtist) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                case DataBaseService::SortByAblumDES: {
                    if (addMeta.pinyinAlbum <= meta.pinyinAlbum) {
                        insertRow(rowIndex, addMeta);
                        isInserted = true;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            if (!isInserted) {
                insertRow(m_model->rowCount(), addMeta);
            }
        }
    }
}

void PlayListView::slotScrollToCurrentPosition(QString songlistHash)
{
    qDebug() << __FUNCTION__ << songlistHash;
    // listmode情况下跳转到播放位置
    if (songlistHash == "all" && this->viewMode() == QListView::ListMode) {
        int height = 0;
        QString currentMetaHash = Player::getInstance()->getActiveMeta().hash;
        for (int i = 0; i < m_model->rowCount(); i++) {
            QModelIndex idx = m_model->index(i, 0, QModelIndex());
            QSize size = m_delegate->sizeHint(QStyleOptionViewItem(), idx);
            MediaMeta meta = idx.data(Qt::UserRole).value<MediaMeta>();
            if (meta.hash == currentMetaHash) {
                this->verticalScrollBar()->setValue(height);
                break;
            }
            height += size.height();
        }
    }
}

void PlayListView::slotAddToPlayQueue()
{
    QItemSelectionModel *selection = selectionModel();
    for (int i = 0; i < selection->selectedRows().size(); i++) {
        QModelIndex curIndex = selection->selectedRows().at(i);
        Player::getInstance()->playListAppendMeta(curIndex.data(Qt::UserRole).value<MediaMeta>());
    }

    Player::getInstance()->signalPlayListChanged();
}

void PlayListView::slotPlayMusic()
{
    slotOnDoubleClicked(this->currentIndex());
}

void PlayListView::showDetailInfoDlg()
{
    if (!m_pInfoDlg) {
        m_pInfoDlg = new InfoDialog(this);
        AC_SET_OBJECT_NAME(m_pInfoDlg, AC_infoDialog);
        AC_SET_ACCESSIBLE_NAME(m_pInfoDlg, AC_infoDialog);
    }

    QModelIndex mindex =  this->currentIndex();
    MediaMeta meta = mindex.data(Qt::UserRole).value<MediaMeta>();
    m_pInfoDlg->updateInfo(meta);
    m_pInfoDlg->show();
}

void PlayListView::slotPlayQueueMetaRemove(QString metaHash)
{
    if (!m_IsPlayQueue) {
        return;
    }
    int row =  m_model->rowCount();
    for (int i = 0; i < row; i++) {
        QModelIndex mindex = m_model->index(i, 0, QModelIndex());
        MediaMeta meta = mindex.data(Qt::UserRole).value<MediaMeta>();
        if (meta.hash == metaHash) {
            m_model->removeRow(i);
            update();
            break;
        }
    }
}

void PlayListView::slotAddToFavSongList(const QString songName)
{
    QList<MediaMeta> listMeta;
    QItemSelectionModel *selection = selectionModel();
    for (int i = 0; i < selection->selectedRows().size(); i++) {
        QModelIndex curIndex = selection->selectedRows().at(i);
        MediaMeta meta = curIndex.data(Qt::UserRole).value<MediaMeta>();
        if (meta.mmType != MIMETYPE_CDA)
            listMeta << curIndex.data(Qt::UserRole).value<MediaMeta>();
    }

    if (listMeta.size() == 0)
        return;
    int insertCount = DataBaseService::getInstance()->addMetaToPlaylist("fav", listMeta);
    emit CommonService::getInstance()->signalShowPopupMessage(songName, selection->selectedRows().size(), insertCount);
    emit CommonService::getInstance()->signalFluashFavoriteBtnIcon();
}

void PlayListView::slotAddToNewSongList(const QString songName)
{
    QItemSelectionModel *selection = selectionModel();
    QList<MediaMeta> metaList;
    for (int i = 0; i < selection->selectedRows().size(); i++) {
        QModelIndex curIndex = selection->selectedRows().at(i);
        MediaMeta meta = curIndex.data(Qt::UserRole).value<MediaMeta>();
        if (meta.mmType != MIMETYPE_CDA)
            metaList.append(meta);
    }

    emit CommonService::getInstance()->signalAddNewSongList();

    if (metaList.size() > 0) {
        QList<DataBaseService::PlaylistData> customSongList = DataBaseService::getInstance()->getCustomSongList();
        QString songlistUuid = customSongList.last().uuid;
        int insertCount = DataBaseService::getInstance()->addMetaToPlaylist(songlistUuid, metaList);
        // 消息通知
        CommonService::getInstance()->signalShowPopupMessage(songName, metaList.size(), insertCount);
        // 刷新自定义歌单页面
        emit CommonService::getInstance()->signalSwitchToView(CustomType, songlistUuid);
    }
}

void PlayListView::slotAddToCustomSongList()
{
    QAction *obj =   dynamic_cast<QAction *>(sender());
    QString songlistHash = obj->data().value<QString>();
    QList<MediaMeta> metas;
    QModelIndexList mindexlist =  this->selectedIndexes();
    for (QModelIndex mindex : mindexlist) {
        MediaMeta imt = mindex.data(Qt::UserRole).value<MediaMeta>();
        if (imt.mmType != MIMETYPE_CDA)
            metas.append(imt);
    }
    if (metas.size() == 0)
        return;
    int insertCount = DataBaseService::getInstance()->addMetaToPlaylist(songlistHash, metas);
    emit CommonService::getInstance()->signalShowPopupMessage(obj->property("displayName").toString(), metas.size(), insertCount);
}

void PlayListView::slotOpenInFileManager()
{
    QModelIndexList modellist = selectionModel()->selectedRows();
    if (modellist.size() == 0)
        return;
    MediaMeta imt = modellist.at(0).data(Qt::UserRole).value<MediaMeta>();
    auto dirUrl = QUrl::fromLocalFile(imt.localPath);
    Dtk::Widget::DDesktopServices::showFileItem(dirUrl);
}

void PlayListView::slotRmvFromSongList()
{
    QModelIndexList modellist = selectionModel()->selectedRows();
    if (modellist.size() == 0)
        return;

    QStringList metaList;
    for (QModelIndex mindex : modellist) {
        MediaMeta imt = mindex.data(Qt::UserRole).value<MediaMeta>();
        //过滤cda格式歌曲
        if (imt.mmType != MIMETYPE_CDA) {
            metaList << imt.hash;
        }
    }

    //过滤cda格式以后再判断size
    if (metaList.size() == 0)
        return;
    Dtk::Widget::DDialog warnDlg(this);
    warnDlg.setObjectName("MessageBox");
    warnDlg.setTextFormat(Qt::RichText);
    warnDlg.addButton(tr("Cancel"), false, Dtk::Widget::DDialog::ButtonNormal);
    warnDlg.addButton(tr("Remove"), true, Dtk::Widget::DDialog::ButtonWarning); //index 1
    MediaMeta meta = modellist.first().data(Qt::UserRole).value<MediaMeta>();
    if (1 == metaList.length()) {
        warnDlg.setMessage(QString(tr("Are you sure you want to remove %1?")).arg(meta.title));
    } else {
        warnDlg.setMessage(QString(tr("Are you sure you want to remove the selected %1 songs?").arg(metaList.size())));
    }

    warnDlg.setIcon(QIcon::fromTheme("deepin-music"));
    if (warnDlg.exec()) { //Remove button
        //数据库中删除时有信号通知刷新界面
        if (!m_IsPlayQueue) {
            if (m_currentHash == "musicResult"
                    || m_currentHash == "album"
                    || m_currentHash == "artist") {
                //搜索结果中删除，等同于所有音乐中删除
                DataBaseService::getInstance()->removeSelectedSongs("all", metaList, false);
                this->clearSelection();
            } else {
                DataBaseService::getInstance()->removeSelectedSongs(m_currentHash, metaList, false);
                this->clearSelection();
            }
            // 更新player中缓存的歌曲信息
            if (m_currentHash == "all" || m_currentHash == Player::getInstance()->getCurrentPlayListHash()) {
                Player::getInstance()->playRmvMeta(metaList);
            }
        } else {
            Player::getInstance()->playRmvMeta(metaList);
        }
    }
}

void PlayListView::slotDelFromLocal()
{
    QModelIndexList mindexlist =  this->selectedIndexes();
    QStringList strlist;
    for (QModelIndex mindex : mindexlist) {
        MediaMeta imt = mindex.data(Qt::UserRole).value<MediaMeta>();
        //过滤cda格式歌曲
        if (imt.mmType != MIMETYPE_CDA) {
            strlist << imt.hash;
        }
    }

    if (strlist.size() == 0)
        return;
    Dtk::Widget::DDialog warnDlg(this);
    warnDlg.setObjectName("MessageBox");
    warnDlg.setTextFormat(Qt::RichText);
    warnDlg.addButton(tr("Cancel"), true, Dtk::Widget::DDialog::ButtonNormal);
    int deleteFlag = warnDlg.addButton(tr("Delete"), false, Dtk::Widget::DDialog::ButtonWarning);

    if (1 == strlist.length()) {
        QModelIndex idx =  this->selectedIndexes().at(0);
        MediaMeta imt = idx.data(Qt::UserRole).value<MediaMeta>();
        warnDlg.setMessage(QString(tr("Are you sure you want to delete %1?")).arg(imt.title));
    } else {
        //                warnDlg.setTitle(QString(tr("Are you sure you want to delete the selected %1 songs?")).arg(metalist.length()));
        DLabel *t_titleLabel = new DLabel(this);
        t_titleLabel->setForegroundRole(DPalette::TextTitle);
        DLabel *t_infoLabel = new DLabel(this);
        t_infoLabel->setForegroundRole(DPalette::TextTips);
        t_titleLabel->setText(tr("Are you sure you want to delete the selected %1 songs?").arg(strlist.length()));
        t_infoLabel->setText(tr("The song files contained will also be deleted"));
        warnDlg.addContent(t_titleLabel, Qt::AlignHCenter);
        warnDlg.addContent(t_infoLabel, Qt::AlignHCenter);
        warnDlg.addSpacing(20);
    }

    warnDlg.setIcon(QIcon::fromTheme("deepin-music"));
    if (deleteFlag == warnDlg.exec()) {
        DataBaseService::getInstance()->removeSelectedSongs("all", strlist, true);
        // 更新player中缓存的歌曲信息，如果存在正在播放的歌曲，停止播放
        Player::getInstance()->playRmvMeta(strlist);
    }
}

void PlayListView::insertRow(int row, MediaMeta meta)
{
    QStandardItem *newItem = new QStandardItem;

    QString imagesDirPath = Global::cacheDir() + "/images/" + meta.hash + ".jpg";
    QFileInfo file(imagesDirPath);
    QIcon icon;
    if (file.exists()) {
        icon = QIcon(imagesDirPath);
    } else {
        icon = QIcon::fromTheme("cover_max");
    }
    newItem->setIcon(icon);
    m_model->insertRow(row, newItem);

    QModelIndex index = m_model->index(row, 0, QModelIndex());
    QVariant mediaMeta;
    mediaMeta.setValue(meta);
    m_model->setData(index, mediaMeta, Qt::UserRole);
}

bool PlayListView::isContain(QString hash)
{
    bool bIsContain = false;
    int row =  m_model->rowCount();
    for (int i = 0; i < row; i++) {
        QModelIndex mindex = m_model->index(i, 0, QModelIndex());
        MediaMeta meta = mindex.data(Qt::UserRole).value<MediaMeta>();
        if (meta.hash == hash) {
            bIsContain = true;
            break;
        }
    }
    return bIsContain;
}

void PlayListView::keyPressEvent(QKeyEvent *event)
{
    switch (event->modifiers()) {
    case Qt::NoModifier:
        switch (event->key()) {
        case Qt::Key_Return: {
            QItemSelectionModel *selection = this->selectionModel();
            if (!selection->selectedRows().isEmpty()) {
                QModelIndex index = selection->selectedRows().first();
                MediaMeta meta = index.data(Qt::UserRole).value<MediaMeta>();

                playMusic(meta);
            }
            break;
        }
        case Qt::Key_Delete: {
            slotRmvFromSongList();
        }
        }
        break;
//    case Qt::ShiftModifier:
//        switch (event->key()) {
//        case Qt::Key_Delete:
//            break;
//        }
//        break;
//    case Qt::ControlModifier:
//        switch (event->key()) {
//        case Qt::Key_I:
//            QItemSelectionModel *selection = this->selectionModel();
//            if (selection->selectedRows().length() <= 0) {
//                return;
//            }
//            auto index = selection->selectedRows().first();
//            auto meta = d->model->meta(index);
//            Q_EMIT showInfoDialog(meta);
//            break;
//        }
//        break;
    default:
        break;
    }

    QAbstractItemView::keyPressEvent(event);
}

//void PlayListView::keyboardSearch(const QString &search)
//{
//    Q_UNUSED(search);
//}

void PlayListView::contextMenuEvent(QContextMenuEvent *event)
{
    QItemSelectionModel *selection = selectionModel();

    if (selection->selectedRows().size() <= 0) {
        return;
    }

    //查找是否有cda格式歌曲
    QStringList metalist;
    foreach (QModelIndex mindex, selection->selectedRows()) {
        MediaMeta meta = mindex.data(Qt::UserRole).value<MediaMeta>();
        if (meta.mmType != MIMETYPE_CDA)
            metalist << meta.hash;
    }

    //全是cda歌曲不处理右键菜单
    if (metalist.size() == 0)
        return;

    QPoint globalPos = mapToGlobal(event->pos());

    DMenu allMusicMenu;//first level menu
    DMenu playlistMenu;//second level menu
    DMenu textCodecMenu; //coding of song information
    QAction *actRmv = nullptr; //remove action
    QAction *actDel = nullptr;

    if (!m_IsPlayQueue) {
        playlistMenu.addAction(tr("Play queue"))->setData("play queue");
        playlistMenu.addSeparator();
    }
    QAction *actFav = playlistMenu.addAction(tr("My favorites"));
    actFav->setData("fav");

    playlistMenu.addSeparator();
    playlistMenu.addAction(tr("Add to new playlist"))->setData("song list");
    playlistMenu.addSeparator();

    //add custom playlist to second menu
    QList<DataBaseService::PlaylistData> strplaylist = DataBaseService::getInstance()->getCustomSongList();
    for (DataBaseService::PlaylistData pd : strplaylist) {
        if (m_currentHash != pd.uuid) { //filter itself
            QFontMetrics titleFm(actFav->font());
            QString displayName = pd.displayName;
            QString text = titleFm.elidedText(QString(pd.displayName.replace("&", "&&")), Qt::ElideMiddle, 170);

            QAction *pact = playlistMenu.addAction(text);

            pact->setProperty("displayName", displayName);
            pact->setData(QVariant(pd.uuid)); //to know which custom view to reach

            connect(pact, SIGNAL(triggered()), this, SLOT(slotAddToCustomSongList()));
        }
    }


    if (selection->selectedRows().size() == 1) {// 选中一首歌
        QModelIndex curIndex = selection->selectedRows().at(0);
        MediaMeta currMeta = curIndex.data(Qt::UserRole).value<MediaMeta>();

        // 收藏按钮是否可以点击
        if (DataBaseService::getInstance()->favoriteExist(currMeta)) {
            actFav->setEnabled(false);
        }

        // 播放或则暂停按钮
        QAction *actplay = nullptr;
        if (currMeta.hash != Player::getInstance()->getActiveMeta().hash ||
                Player::getInstance()->status() != Player::Playing) {
            actplay = allMusicMenu.addAction(tr("Play"));
            connect(actplay, &QAction::triggered, this, &PlayListView::slotPlayMusic);
        } else {
            actplay = allMusicMenu.addAction(tr("Pause"));
            connect(actplay, &QAction::triggered, Player::getInstance(), &Player::pause);
        }

        if (currMeta.invalid)
            actplay->setEnabled(false);

        allMusicMenu.addAction(tr("Add to playlist"))->setMenu(&playlistMenu);
        allMusicMenu.addSeparator();
        QAction *actdisplay = allMusicMenu.addAction(tr("Display in file manager"));
        if (m_IsPlayQueue) {
            actRmv = allMusicMenu.addAction(tr("Remove from play queue"));
        } else {
            actRmv = allMusicMenu.addAction(tr("Remove from playlist"));
        }
        actDel = allMusicMenu.addAction(tr("Delete from local disk"));

        allMusicMenu.addSeparator();

        QList<QByteArray> codecList = MetaDetector::detectEncodings(currMeta);

        if (!codecList.contains("UTF-8")) {
            codecList.push_front("UTF-8");
        }
        if (QLocale::system().name() == "zh_CN") {
            if (codecList.contains("GB18030")) {
                codecList.removeAll("GB18030");
            }

            if (!codecList.isEmpty()) {
                codecList.push_front("GB18030");
            }
        }

        for (auto codec : codecList) {
            auto act = textCodecMenu.addAction(codec);
            act->setParent(&textCodecMenu);
            act->setCheckable(true);

            if (codec == currMeta.codec) {
                act->setChecked(true);
            }

            act->setData(QVariant::fromValue(codec));
        }

        if (codecList.length() >= 1) {
            allMusicMenu.addSeparator();
            allMusicMenu.addAction(tr("Encoding"))->setMenu(&textCodecMenu);
        }

        allMusicMenu.addSeparator();
        auto actsonginfo = allMusicMenu.addAction(tr("Song info"));

        //connnect
        connect(actdisplay, SIGNAL(triggered()), this, SLOT(slotOpenInFileManager()));
        connect(actsonginfo, SIGNAL(triggered()), this, SLOT(showDetailInfoDlg()));
        connect(&textCodecMenu, &QMenu::triggered, this, &PlayListView::slotTextCodecMenuClicked);
    } else {// 选中多首歌

        // 收藏按钮是否可以点击
        actFav->setEnabled(false);
        for (int i = 0; i < selection->selectedRows().size(); i++) {
            QModelIndex curIndex = selection->selectedRows().at(i);
            MediaMeta currMeta = curIndex.data(Qt::UserRole).value<MediaMeta>();

            if (!DataBaseService::getInstance()->favoriteExist(currMeta)) {
                actFav->setEnabled(true);
                break;
            }
        }

        allMusicMenu.addAction(tr("Add to playlist"))->setMenu(&playlistMenu);
        if (m_IsPlayQueue) {
            actRmv = allMusicMenu.addAction(tr("Remove from play queue"));
        } else {
            actRmv = allMusicMenu.addAction(tr("Remove from playlist"));
        }
        actDel = allMusicMenu.addAction(tr("Delete from local disk"));
    }

    connect(actRmv, SIGNAL(triggered()), this, SLOT(slotRmvFromSongList()));
    connect(actDel, SIGNAL(triggered()), this, SLOT(slotDelFromLocal()));
    connect(&playlistMenu, &QMenu::triggered, this, &PlayListView::slotPlaylistMenuClicked);

    allMusicMenu.exec(globalPos);
}

void PlayListView::dragMoveEvent(QDragMoveEvent *event)
{
    auto index = indexAt(event->pos());
    if (/*index.isValid() && */(event->mimeData()->hasFormat("text/uri-list"))) {
        event->setDropAction(Qt::CopyAction);
        event->acceptProposedAction();
    } else {
        DListView::dragMoveEvent(event);
    }
}

void PlayListView::dropEvent(QDropEvent *event)
{
    if ((!event->mimeData()->hasFormat("text/uri-list"))) {
        return;
    }

    if (event->mimeData()->hasFormat("text/uri-list")) {
        auto urls = event->mimeData()->urls();
        QStringList localpaths;
        for (auto &url : urls) {
            localpaths << url.toLocalFile();
        }

        if (!localpaths.isEmpty() && m_currentHash != "CdaRole") { //cda歌单不需要添加歌曲
            DataBaseService::getInstance()->importMedias(m_currentHash, localpaths);
        }
    }

    DListView::dropEvent(event);
}

bool PlayListView::getIsPlayQueue() const
{
    return m_IsPlayQueue;
}

//void PlayListView::reflushItemMediaMeta(const MediaMeta &meta)
//{
//    for (int i = 0; i <  m_model->rowCount(); i++) {
//        QModelIndex curIndex = m_model->index(i, 0);
//        MediaMeta metaTemp = curIndex.data(Qt::UserRole).value<MediaMeta>();

//        if (meta.hash == metaTemp.hash) {
//            QVariant mediaMeta;
//            mediaMeta.setValue(meta);
//            m_model->setData(curIndex, mediaMeta, Qt::UserRole);
//            break;
//        }
//    }
//}

void PlayListView::slotTextCodecMenuClicked(QAction *action)
{
    QItemSelectionModel *selection = selectionModel();
    if (selection->selectedRows().size() > 0) {
        QModelIndex curIndex = selection->selectedRows().at(0);
        MediaMeta meta = curIndex.data(Qt::UserRole).value<MediaMeta>();

        qDebug() << action->data().toByteArray();
        meta.codec = action->data().toByteArray();
        meta.updateCodec(action->data().toByteArray());
        QVariant varmeta;
        varmeta.setValue(meta);
        m_model->setData(curIndex, varmeta, Qt::UserRole);

        //restore to db
        DataBaseService::getInstance()->updateMetaCodec(meta);

        //同步编码到其他页面
        CommonService::getInstance()->signalUpdateCodec(meta);
    }
}

void PlayListView::slotPlaylistMenuClicked(QAction *action)
{
    QString actionText = action->data().toString();
    if (actionText == "fav") {
        slotAddToFavSongList(action->text());
    } else if (actionText == "song list") {
        slotAddToNewSongList(action->text());
    } else if (actionText == "play queue") {
        slotAddToPlayQueue();
    }
}

void PlayListView::slotUpdateCodec(const MediaMeta &meta)
{
    for (int i = 0; i < m_model->rowCount(); i++) {
        MediaMeta tmpmeta = m_model->index(i, 0).data(Qt::UserRole).value<MediaMeta>();
        if (meta.hash == tmpmeta.hash) {
            QVariant varmeta;
            varmeta.setValue(meta);
            m_model->setData(m_model->index(i, 0), varmeta, Qt::UserRole);
            return;
        }
    }
}

void PlayListView::rmvCdaSongs()
{
    //remove cda songs
    if (m_IsPlayQueue) {
        for (int i = 0; i < m_model->rowCount();) {
            MediaMeta tmpmeta = m_model->index(i, 0).data(Qt::UserRole).value<MediaMeta>();
            if (tmpmeta.mmType == MIMETYPE_CDA) {
                m_model->removeRow(i);
            } else {
                i++;
            }
        }
    }
}

void PlayListView::playMusic(const MediaMeta &meta)
{
    if (Player::getInstance()->getActiveMeta().hash == meta.hash) {
        if (Player::getInstance()->status() == Player::Paused) {
            Player::getInstance()->resume();
        } else if (Player::getInstance()->status() == Player::Stopped) {
            Player::getInstance()->playMeta(meta);
            // 通知播放队列列表改变
            emit Player::getInstance()->signalPlayListChanged();
            // 设置当前播放playlist的hash
            Player::getInstance()->setCurrentPlayListHash(m_currentHash, false);
        }
    } else {
        if (!m_IsPlayQueue) {
            // 设置新的播放列表
            Player::getInstance()->clearPlayList();
            Player::getInstance()->setPlayList(getMusicListData());
            // 通知播放队列列表改变
            emit Player::getInstance()->signalPlayListChanged();
            // 设置当前播放playlist的hash
            Player::getInstance()->setCurrentPlayListHash(m_currentHash, false);
        }
        Player::getInstance()->playMeta(meta);
    }
}

void PlayListView::mouseMoveEvent(QMouseEvent *event)
{
    DListView::mouseMoveEvent(event);
}

void PlayListView::dragEnterEvent(QDragEnterEvent *event)
{
    auto t_formats = event->mimeData()->formats();
    if (event->mimeData()->hasFormat("text/uri-list") || event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
        event->setDropAction(Qt::CopyAction);
        event->acceptProposedAction();
    }
}

void PlayListView::startDrag(Qt::DropActions supportedActions)
{
    QItemSelection selection;

    for (QModelIndex index : selectionModel()->selectedIndexes()) {
        selection.append(QItemSelectionRange(index));
    }

    if (!selectionModel()->selectedIndexes().isEmpty())
        scrollTo(selectionModel()->selectedIndexes().first());
    setAutoScroll(false);
    DListView::startDrag(supportedActions);
    setAutoScroll(true);

    if (!selection.isEmpty()) {
        selectionModel()->select(selection, QItemSelectionModel::Select);
    }
}

