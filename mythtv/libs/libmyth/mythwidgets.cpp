#include <iostream>
using namespace std;

#include <QStyle>
#include <QPainter>
#include <QCursor>
#include <QLayout>
#include <QKeyEvent>
#include <QHideEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QEvent>

#include "mythwidgets.h"
#include "mythcontext.h"
#include "mythmiscutil.h"
#include "mythdialogs.h"
#include "mythlogging.h"
#include "mythmainwindow.h"
#include "mythactions.h"

typedef VirtualKeyboardQt* QWidgetP;
static void qt_delete(QWidgetP &widget)
{
    if (widget)
    {
        widget->disconnect();
        widget->hide();
        widget->deleteLater();
        widget = NULL;
    }
}

static struct ActionDefStruct<MythComboBox> mcbActions[] = {
    { "UP",       &MythComboBox::doUp },
    { "DOWN",     &MythComboBox::doDown },
    { "LEFT",     &MythComboBox::doLeft },
    { "RIGHT",    &MythComboBox::doRight },
    { "PAGEDOWN", &MythComboBox::doPgDown },
    { "PAGEUP",   &MythComboBox::doPgUp },
    { "SELECT",   &MythComboBox::doSelect }
};
static int mcbActionCount = NELEMS(mcbActions);


MythComboBox::MythComboBox(bool rw, QWidget *parent, const char *name) :
    QComboBox(parent),
    popup(NULL), helptext(QString::null), AcceptOnSelect(false),
    useVirtualKeyboard(true), allowVirtualKeyboard(rw),
    popupPosition(VKQT_POSBELOWEDIT), step(1),
    m_actions(new MythActions<MythComboBox>(this, mcbActions, mcbActionCount))
{
    setObjectName(name);
    setEditable(rw);
    useVirtualKeyboard = gCoreContext->GetNumSetting("UseVirtualKeyboard", 1);
}

MythComboBox::~MythComboBox()
{
    if (!m_actions)
        delete m_actions;

    Teardown();
}

void MythComboBox::deleteLater(void)
{
    Teardown();
    QComboBox::deleteLater();
}

void MythComboBox::Teardown(void)
{
    qt_delete(popup);
}

void MythComboBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythComboBox::popupVirtualKeyboard(void)
{
    qt_delete(popup);

    popup = new VirtualKeyboardQt(GetMythMainWindow(), this);
    GetMythMainWindow()->detach(popup);
    popup->exec();

    qt_delete(popup);
}

bool MythComboBox::doUp(const QString &action)
{
    focusNextPrevChild(false);
    return true;
}

bool MythComboBox::doDown(const QString &action)
{
    focusNextPrevChild(true);
    return true;
}

bool MythComboBox::doLeft(const QString &action)
{
    if (currentIndex() == 0)
        setCurrentIndex(count()-1);
    else if (count() > 0)
        setCurrentIndex((currentIndex() - 1) % count());

    emit activated(currentIndex());
    emit activated(itemText(currentIndex()));
    return true;
}

bool MythComboBox::doRight(const QString &action)
{
    if (count() > 0)
        setCurrentIndex((currentIndex() + 1) % count());

    emit activated(currentIndex());
    emit activated(itemText(currentIndex()));
    return true;
}

bool MythComboBox::doPgDown(const QString &action)
{
    if (currentIndex() == 0)
        setCurrentIndex(count() - (step % count()));
    else if (count() > 0)
        setCurrentIndex((currentIndex() + count() - (step % count())) %
                        count());

    emit activated(currentIndex());
    emit activated(itemText(currentIndex()));
    return true;
}

bool MythComboBox::doPgUp(const QString &action)
{
    if (count() > 0)
        setCurrentIndex((currentIndex() + (step % count())) % count());

    emit activated(currentIndex());
    emit activated(itemText(currentIndex()));
    return true;
}

bool MythComboBox::doSelect(const QString &action)
{
    if (AcceptOnSelect)
        emit accepted(currentIndex());
    else if ((m_actionEvent->text().isEmpty() ||
              (m_actionEvent->key() == Qt::Key_Enter) ||
              (m_actionEvent->key() == Qt::Key_Return) ||
              (m_actionEvent->key() == Qt::Key_Space)) &&
              useVirtualKeyboard && allowVirtualKeyboard)
        popupVirtualKeyboard();
    return true;
}

void MythComboBox::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions,
                                                     !allowVirtualKeyboard);

    if ((!popup || popup->isHidden()) && !handled)
    {
        m_actionEvent  = e;
        handled = m_actions->handleActions(actions);
    }

    if (!handled)
    {
        if (isEditable())
            QComboBox::keyPressEvent(e);
        else
            e->ignore();
    }
}

void MythComboBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    if (lineEdit())
        lineEdit()->setPalette(palette);

    QComboBox::focusInEvent(e);
}

void MythComboBox::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());

    if (lineEdit())
    {
        lineEdit()->setPalette(QPalette());

        // commit change if the user was editing an entry
        QString curText = currentText();
        int i;
        bool foundItem = false;

        for(i = 0; i < count(); i++)
            if (curText == itemText(i))
                foundItem = true;

        if (!foundItem)
        {
            insertItem(curText);
            setCurrentIndex(count()-1);
        }
    }

    QComboBox::focusOutEvent(e);
}

static struct ActionDefStruct<MythCheckBox> mchkbActions[] = {
    { "UP",       &MythCheckBox::doUp },
    { "DOWN",     &MythCheckBox::doDown },
    { "LEFT",     &MythCheckBox::doToggle },
    { "RIGHT",    &MythCheckBox::doToggle },
    { "SELECT",   &MythCheckBox::doToggle }
};
static int mchkbActionCount = NELEMS(mchkbActions);

MythCheckBox::MythCheckBox(QWidget *parent, const char *name) :
    QCheckBox(parent),
    m_actions(new MythActions<MythCheckBox>(this, mchkbActions,
                                            mchkbActionCount))
{
    setObjectName(name);
}

MythCheckBox::MythCheckBox(const QString &text, QWidget *parent,
                           const char *name) : QCheckBox(text, parent),
    m_actions(new MythActions<MythCheckBox>(this, mchkbActions,
                                            mchkbActionCount))
{
    setObjectName(name);
}

MythCheckBox::~MythCheckBox()
{
    if (m_actions)
        delete m_actions;
}

bool MythCheckBox::doUp(const QString &action)
{
    focusNextPrevChild(false);
    return true;
}

bool MythCheckBox::doDown(const QString &action)
{
    focusNextPrevChild(true);
    return true;
}

bool MythCheckBox::doToggle(const QString &action)
{
    toggle();
    return true;
}

void MythCheckBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;

    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    if (!handled)
        handled = m_actions->handleActions(actions);

    if (!handled)
        e->ignore();
}

void MythCheckBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythCheckBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);
    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QCheckBox::focusInEvent(e);
}

void MythCheckBox::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QCheckBox::focusOutEvent(e);
}

static struct ActionDefStruct<MythRadioButton> mrbActions[] = {
    { "UP",       &MythRadioButton::doUp },
    { "DOWN",     &MythRadioButton::doDown },
    { "LEFT",     &MythRadioButton::doToggle },
    { "RIGHT",    &MythRadioButton::doToggle }
};
static int mrbActionCount = NELEMS(mrbActions);

MythRadioButton::MythRadioButton(QWidget* parent, const char* name) :
    QRadioButton(parent),
    m_actions(new MythActions<MythRadioButton>(this, mrbActions,
                                               mrbActionCount))
{
    setObjectName(name);
}

MythRadioButton::~MythRadioButton()
{
    if (m_actions)
        delete m_actions;
}

bool MythRadioButton::doUp(const QString &action)
{
    focusNextPrevChild(false);
    return true;
}

bool MythRadioButton::doDown(const QString &action)
{
    focusNextPrevChild(true);
    return true;
}

bool MythRadioButton::doToggle(const QString &action)
{
    toggle();
    return true;
}

void MythRadioButton::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    if (!handled)
        handled = m_actions->handleActions(actions);

    if (!handled)
        e->ignore();
}

void MythRadioButton::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythRadioButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QRadioButton::focusInEvent(e);
}

void MythRadioButton::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QRadioButton::focusOutEvent(e);
}


static struct ActionDefStruct<MythSpinBox> msbActions[] = {
    { "UP",       &MythSpinBox::doUp },
    { "DOWN",     &MythSpinBox::doDown },
    { "LEFT",     &MythSpinBox::doLeft },
    { "RIGHT",    &MythSpinBox::doRight },
    { "PAGEDOWN", &MythSpinBox::doPgDown },
    { "PAGEUP",   &MythSpinBox::doPgUp },
    { "SELECT",   &MythSpinBox::doSelect }
};
static int msbActionCount = NELEMS(msbActions);

MythSpinBox::MythSpinBox(QWidget* parent, const char* name,
                bool allow_single_step) :
    QSpinBox(parent), allowsinglestep(allow_single_step),
    m_actions(new MythActions<MythSpinBox>(this, msbActions, msbActionCount))
{
    setObjectName(name);
    if (allowsinglestep)
        setSingleStep(10);
}

MythSpinBox::~MythSpinBox()
{
    if (m_actions)
        delete m_actions;
}

bool MythSpinBox::doUp(const QString &action)
{
    focusNextPrevChild(false);
    return true;
}

bool MythSpinBox::doDown(const QString &action)
{
    focusNextPrevChild(true);
    return true;
}

bool MythSpinBox::doLeft(const QString &action)
{
    allowsinglestep ? setValue(value()-1) : stepDown();
    return true;
}

bool MythSpinBox::doRight(const QString &action)
{
    allowsinglestep ? setValue(value()+1) : stepUp();
    return true;
}

bool MythSpinBox::doPgDown(const QString &action)
{
    stepDown();
    return true;
}

bool MythSpinBox::doPgUp(const QString &action)
{
    stepUp();
    return true;
}

bool MythSpinBox::doSelect(const QString &action)
{
    return true;
}

void MythSpinBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythSpinBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    if (!handled)
        handled = m_actions->handleActions(actions);

    if (!handled)
        QSpinBox::keyPressEvent(e);
}

void MythSpinBox::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QSpinBox::focusInEvent(e);
}

void MythSpinBox::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QSpinBox::focusOutEvent(e);
}

static struct ActionDefStruct<MythSlider> mslideActions[] = {
    { "UP",       &MythSlider::doUp },
    { "DOWN",     &MythSlider::doDown },
    { "LEFT",     &MythSlider::doLeft },
    { "RIGHT",    &MythSlider::doRight },
    { "SELECT",   &MythSlider::doSelect }
};
static int mslideActionCount = NELEMS(mslideActions);

MythSlider::MythSlider(QWidget* parent, const char* name) :
    QSlider(parent),
    m_actions(new MythActions<MythSlider>(this, mslideActions,
                                          mslideActionCount))
{
    setObjectName(name);
}

MythSlider::~MythSlider()
{
    if (m_actions)
        delete m_actions;
}

bool MythSlider::doUp(const QString &action)
{
    focusNextPrevChild(false);
    return true;
}

bool MythSlider::doDown(const QString &action)
{
    focusNextPrevChild(true);
    return true;
}

bool MythSlider::doLeft(const QString &action)
{
    setValue(value() - singleStep());
    return true;
}

bool MythSlider::doRight(const QString &action)
{
    setValue(value() + singleStep());
    return true;
}

bool MythSlider::doSelect(const QString &action)
{
    return true;
}


void MythSlider::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    if (!handled)
        handled = m_actions->handleActions(actions);

    if (!handled)
        QSlider::keyPressEvent(e);
}

void MythSlider::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythSlider::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QSlider::focusInEvent(e);
}

void MythSlider::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QSlider::focusOutEvent(e);
}


static struct ActionDefStruct<MythLineEdit> mleActions[] = {
    { "UP",       &MythLineEdit::doUp },
    { "DOWN",     &MythLineEdit::doDown },
    { "SELECT",   &MythLineEdit::doSelect }
};
static int mleActionCount = NELEMS(mleActions);

MythLineEdit::MythLineEdit(QWidget *parent, const char *name) :
    QLineEdit(parent),
    popup(NULL), helptext(QString::null), rw(true),
    useVirtualKeyboard(true),
    allowVirtualKeyboard(true),
    popupPosition(VKQT_POSBELOWEDIT),
    m_actions(new MythActions<MythLineEdit>(this, mleActions, mleActionCount))
{
    setObjectName(name);
    useVirtualKeyboard = gCoreContext->GetNumSetting("UseVirtualKeyboard", 1);
}

MythLineEdit::MythLineEdit(const QString &contents, QWidget *parent,
                           const char *name) :
    QLineEdit(contents, parent),
    popup(NULL), helptext(QString::null), rw(true),
    useVirtualKeyboard(true),
    allowVirtualKeyboard(true),
    popupPosition(VKQT_POSBELOWEDIT),
    m_actions(new MythActions<MythLineEdit>(this, mleActions, mleActionCount))
{
    setObjectName(name);
    useVirtualKeyboard = gCoreContext->GetNumSetting("UseVirtualKeyboard", 1);
}

MythLineEdit::~MythLineEdit()
{
    if (m_actions)
        delete m_actions;

    Teardown();
}

void MythLineEdit::deleteLater(void)
{
    Teardown();
    QLineEdit::deleteLater();
}

void MythLineEdit::Teardown(void)
{
    qt_delete(popup);
}

void MythLineEdit::popupVirtualKeyboard(void)
{
    qt_delete(popup);

    popup = new VirtualKeyboardQt(GetMythMainWindow(), this);
    GetMythMainWindow()->detach(popup);
    popup->exec();

    qt_delete(popup);
}

bool MythLineEdit::doUp(const QString &action)
{
    focusNextPrevChild(false);
    return true;
}

bool MythLineEdit::doDown(const QString &action)
{
    focusNextPrevChild(true);
    return true;
}

bool MythLineEdit::doSelect(const QString &action)
{
    if (m_actionEvent->text().isEmpty() ||
        (m_actionEvent->key() == Qt::Key_Enter) ||
        (m_actionEvent->key() == Qt::Key_Return))
    {
        if (useVirtualKeyboard && allowVirtualKeyboard && rw)
            popupVirtualKeyboard();
        else
            return false;
    }
    else if (m_actionEvent->text().isEmpty() )
        m_actionEvent->ignore();
    return true;
}

void MythLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions, false);

    if (!handled)
    {
        m_actionEvent = e;
        handled = m_actions->handleActions(actions);
    }

    if (!handled)
        if (rw || e->key() == Qt::Key_Escape || e->key() == Qt::Key_Left
               || e->key() == Qt::Key_Return || e->key() == Qt::Key_Right)
            QLineEdit::keyPressEvent(e);
}

void MythLineEdit::setText(const QString &text)
{
    // Don't mess with the cursor position; it causes
    // counter-intuitive behaviour due to interactions with the
    // communication with the settings stuff

    int pos = cursorPosition();
    QLineEdit::setText(text);
    setCursorPosition(pos);
}

QString MythLineEdit::text(void)
{
    return QLineEdit::text();
}

void MythLineEdit::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QLineEdit::focusInEvent(e);
}

void MythLineEdit::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    if (popup && !popup->isHidden() && !popup->hasFocus())
        popup->hide();
    QLineEdit::focusOutEvent(e);
}

void MythLineEdit::hideEvent(QHideEvent *e)
{
    if (popup && !popup->isHidden())
        popup->hide();
    QLineEdit::hideEvent(e);
}

void MythLineEdit::mouseDoubleClickEvent(QMouseEvent *e)
{
    QLineEdit::mouseDoubleClickEvent(e);
}


static struct ActionDefStruct<MythRemoteLineEdit> mrleActions[] = {
    { "UP",       &MythRemoteLineEdit::doUp },
    { "DOWN",     &MythRemoteLineEdit::doDown },
    { "SELECT",   &MythRemoteLineEdit::doSelect },
    { "ESCAPE",   &MythRemoteLineEdit::doEscape }
};
static int mrleActionCount = NELEMS(mrleActions);


MythRemoteLineEdit::MythRemoteLineEdit(QWidget * parent, const char *name) :
    QTextEdit(parent),
    m_actions(new MythActions<MythRemoteLineEdit>(this, mrleActions,
                                                  mrleActionCount))
{
    setObjectName(name);
    my_font = NULL;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(const QString & contents,
                                       QWidget * parent, const char *name) :
    QTextEdit(parent),
    m_actions(new MythActions<MythRemoteLineEdit>(this, mrleActions,
                                                  mrleActionCount))
{
    setObjectName(name);
    my_font = NULL;
    m_lines = 1;
    this->Init();
    setText(contents);
}

MythRemoteLineEdit::MythRemoteLineEdit(QFont *a_font, QWidget * parent,
                                       const char *name) :
    QTextEdit(parent),
    m_actions(new MythActions<MythRemoteLineEdit>(this, mrleActions,
                                                  mrleActionCount))
{
    setObjectName(name);
    my_font = a_font;
    m_lines = 1;
    this->Init();
}

MythRemoteLineEdit::MythRemoteLineEdit(int lines, QWidget * parent,
                                       const char *name) :
    QTextEdit(parent),
    m_actions(new MythActions<MythRemoteLineEdit>(this, mrleActions,
                                                  mrleActionCount))
{
    setObjectName(name);
    my_font = NULL;
    m_lines = lines;
    this->Init();
}

void MythRemoteLineEdit::Init()
{
    //  Bunch of default values
    cycle_timer = new QTimer();
    shift = false;
    active_cycle = false;
    current_choice = "";
    current_set = "";

    cycle_time = 3000;

    pre_cycle_text_before_cursor = "";
    pre_cycle_text_after_cursor = "";

    setCharacterColors(
        QColor(100, 100, 100), QColor(0, 255, 255), QColor(255, 0, 0));

    //  Try and make sure it doesn't ever change
    setWordWrapMode(QTextOption::NoWrap);

    if (my_font)
        setFont(*my_font);

    QFontMetrics fontsize(font());

    setMinimumHeight(fontsize.height() * 5 / 4);
    setMaximumHeight(fontsize.height() * m_lines * 5 / 4);

    connect(cycle_timer, SIGNAL(timeout()), this, SLOT(endCycle()));

    popup = NULL;
    useVirtualKeyboard = gCoreContext->GetNumSetting("UseVirtualKeyboard", 1);
    popupPosition = VKQT_POSBELOWEDIT;
}


void MythRemoteLineEdit::setCharacterColors(
    QColor unselected, QColor selected, QColor special)
{
    col_unselected = unselected;
    hex_unselected = QString("%1%2%3")
        .arg(col_unselected.red(),   2, 16, QLatin1Char('0'))
        .arg(col_unselected.green(), 2, 16, QLatin1Char('0'))
        .arg(col_unselected.blue(),  2, 16, QLatin1Char('0'));

    col_selected = selected;
    hex_selected = QString("%1%2%3")
        .arg(col_selected.red(),   2, 16, QLatin1Char('0'))
        .arg(col_selected.green(), 2, 16, QLatin1Char('0'))
        .arg(col_selected.blue(),  2, 16, QLatin1Char('0'));

    col_special = special;
    hex_special = QString("%1%2%3")
        .arg(col_special.red(),   2, 16, QLatin1Char('0'))
        .arg(col_special.green(), 2, 16, QLatin1Char('0'))
        .arg(col_special.blue(),  2, 16, QLatin1Char('0'));
}

void MythRemoteLineEdit::startCycle(QString current_choice, QString set)
{
    if (active_cycle)
    {
        LOG(VB_GENERAL, LOG_ALERT, 
                 "startCycle() called, but edit is already in a cycle.");
        return;
    }

    cycle_timer->setSingleShot(true);
    cycle_timer->start(cycle_time);
    active_cycle = true;

    QTextCursor pre_cycle_cursor = textCursor();

    QTextCursor upto_cursor_sel = pre_cycle_cursor;
    upto_cursor_sel.movePosition(
        QTextCursor::NoMove, QTextCursor::MoveAnchor);
    upto_cursor_sel.movePosition(
        QTextCursor::Start,  QTextCursor::KeepAnchor);
    pre_cycle_text_before_cursor = upto_cursor_sel.selectedText();

    QTextCursor from_cursor_sel = pre_cycle_cursor;
    from_cursor_sel.movePosition(
        QTextCursor::NoMove, QTextCursor::MoveAnchor);
    from_cursor_sel.movePosition(
        QTextCursor::End,  QTextCursor::KeepAnchor);
    pre_cycle_text_after_cursor = from_cursor_sel.selectedText();

    pre_cycle_pos = pre_cycle_text_before_cursor.length();

    updateCycle(current_choice, set); // Show the user their options
}

void MythRemoteLineEdit::updateCycle(QString current_choice, QString set)
{
    int index;
    QString aString, bString;

    //  Show the characters in the current set being cycled
    //  through, with the current choice in a different color. If the current
    //  character is uppercase X (interpreted as destructive
    //  backspace) or an underscore (interpreted as a space)
    //  then show these special cases in yet another color.

    if (shift)
    {
        set = set.toUpper();
        current_choice = current_choice.toUpper();
    }

    bString  = "<B>";
    if (current_choice == "_" || current_choice == "X")
    {
        bString += "<FONT COLOR=\"#";
        bString += hex_special;
        bString += "\">";
        bString += current_choice;
        bString += "</FONT>";
    }
    else
    {
        bString += "<FONT COLOR=\"#";
        bString += hex_selected;
        bString += "\">";
        bString += current_choice;
        bString += "</FONT>";
    }
    bString += "</B>";

    index = set.indexOf(current_choice);
    int length = set.length();
    if (index < 0 || index > length)
    {
        LOG(VB_GENERAL, LOG_ALERT,
                 QString("MythRemoteLineEdit passed a choice of \"%1"
                         "\" which is not in set \"%2\"")
                     .arg(current_choice).arg(set));
        setText("????");
        return;
    }
    else
    {
        set.replace(index, current_choice.length(), bString);
    }

    QString esc_upto =  pre_cycle_text_before_cursor;
    QString esc_from =  pre_cycle_text_after_cursor;

    esc_upto.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");
    esc_from.replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");

    aString = esc_upto;
    aString += "<FONT COLOR=\"#";
    aString += hex_unselected;
    aString += "\">[";
    aString += set;
    aString += "]</FONT>";
    aString += esc_from;
    setHtml(aString);

    QTextCursor tmp = textCursor();
    tmp.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    tmp.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                     pre_cycle_pos + set.length());
    setTextCursor(tmp);
    update();

    if (current_choice == "X" && !pre_cycle_text_before_cursor.isEmpty())
    {
        //  If current selection is delete, select the character to be deleted
        QTextCursor tmp = textCursor();
        tmp.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        tmp.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                         pre_cycle_pos - 1);
        tmp.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        setTextCursor(tmp);
    }
    else
    {
        // Restore original cursor location
        QTextCursor tmp = textCursor();
        tmp.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
        tmp.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                         pre_cycle_pos);
        setTextCursor(tmp);
    }
}

void MythRemoteLineEdit::endCycle(bool select)
{
    if (!active_cycle)
        return;

    QString tmpString = "";
    int pos = pre_cycle_pos;

    //  The timer ran out or the user pressed a key
    //  outside of the current set of choices
    if (!select)
    {
        tmpString = pre_cycle_text_before_cursor;
    }
    else if (current_choice == "X") // destructive backspace
    {
        if (!pre_cycle_text_before_cursor.isEmpty())
        {
            tmpString = pre_cycle_text_before_cursor.left(
                pre_cycle_text_before_cursor.length() - 1);
            pos--;
        }
    }
    else
    {
        current_choice = (current_choice == "_") ? " " : current_choice;
        current_choice = (shift) ? current_choice.toUpper() : current_choice;

        tmpString  = pre_cycle_text_before_cursor;
        tmpString += current_choice;
        pos++;
    }

    tmpString += pre_cycle_text_after_cursor;

    setPlainText(tmpString);

    QTextCursor tmpCursor = textCursor();
    tmpCursor.movePosition(QTextCursor::Start, QTextCursor::MoveAnchor);
    tmpCursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, pos);
    setTextCursor(tmpCursor);

    active_cycle                 = false;
    current_choice               = "";
    current_set                  = "";
    pre_cycle_text_before_cursor = "";
    pre_cycle_text_after_cursor  = "";

    if (select)
        emit textChanged(toPlainText());
}

void MythRemoteLineEdit::setText(const QString &text)
{
    QTextEdit::clear();
    QTextEdit::insertPlainText(text);
}

QString MythRemoteLineEdit::text(void)
{
    return QTextEdit::toPlainText();
}

bool MythRemoteLineEdit::doUp(const QString &action)
{
    endCycle();
    // Need to call very base one because
    // QTextEdit reimplements it to tab
    // through links (even if you're in
    // PlainText Mode !!)
    QWidget::focusNextPrevChild(false);
    emit tryingToLooseFocus(false);
    return true;
}

bool MythRemoteLineEdit::doDown(const QString &action)
{
    endCycle();
    QWidget::focusNextPrevChild(true);
    emit tryingToLooseFocus(true);
    return true;
}

bool MythRemoteLineEdit::doSelect(const QString &action)
{
    if ((!active_cycle) && useVirtualKeyboard &&
        ((m_actionEvent->text().isEmpty()) ||
         (m_actionEvent->key() == Qt::Key_Enter) ||
         (m_actionEvent->key() == Qt::Key_Return)))
    {
        popupVirtualKeyboard();
    }
    return true;
}

bool MythRemoteLineEdit::doEscape(const QString &action)
{
    if (active_cycle)
        endCycle(false);

    return true;
}


void MythRemoteLineEdit::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions, false);

    if ((!popup || popup->isHidden()) && !handled)
    {
        m_actionEvent = e;
        handled = m_actions->handleActions(actions);
    }

    if (handled)
        return;

    if (popup && !popup->isHidden())
    {
        endCycle();
        QTextEdit::keyPressEvent(e);
        emit textChanged(toPlainText());
        return;
    }

    switch (e->key())
    {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            handled = true;
            endCycle();
            e->ignore();
            break;

            //  Only eat Qt::Key_Space if we are in a cycle

        case Qt::Key_Space:
            if (active_cycle)
            {
                handled = true;
                endCycle();
                e->ignore();
            }
            break;

            //  If you want to mess around with other ways to allocate
            //  key presses you can just add entries between the quotes

        case Qt::Key_1:
            cycleKeys("_X%-/.?()1");
            handled = true;
            break;

        case Qt::Key_2:
            cycleKeys("abc2");
            handled = true;
            break;

        case Qt::Key_3:
            cycleKeys("def3");
            handled = true;
            break;

        case Qt::Key_4:
            cycleKeys("ghi4");
            handled = true;
            break;

        case Qt::Key_5:
            cycleKeys("jkl5");
            handled = true;
            break;

        case Qt::Key_6:
            cycleKeys("mno6");
            handled = true;
            break;

        case Qt::Key_7:
            cycleKeys("pqrs7");
            handled = true;
            break;

        case Qt::Key_8:
            cycleKeys("tuv8");
            handled = true;
            break;

        case Qt::Key_9:
            cycleKeys("wxyz90");
            handled = true;
            break;

        case Qt::Key_0:
            toggleShift();
            handled = true;
            break;
    }

    if (!handled)
    {
        endCycle();
        QTextEdit::keyPressEvent(e);
        emit textChanged(toPlainText());
    }
}

void MythRemoteLineEdit::setCycleTime(float desired_interval)
{
    if (desired_interval < 0.5 || desired_interval > 10.0)
    {
        LOG(VB_GENERAL, LOG_ALERT, 
                 QString("cycle interval of %1 milliseconds ")
                     .arg((int) (desired_interval * 1000)) +
                 "\n\t\t\tis outside of the allowed range of "
                 "500 to 10,000 milliseconds");
        return;
    }

    cycle_time = (int) (desired_interval * 1000);
}

void MythRemoteLineEdit::cycleKeys(QString cycle_list)
{
    int index;

    if (active_cycle)
    {
        if (cycle_list == current_set)
        {
            //  Regular movement through existing set
            cycle_timer->start(cycle_time);
            index = current_set.indexOf(current_choice);
            int length = current_set.length();
            if (index + 1 >= length)
            {
                index = -1;
            }
            current_choice = current_set.mid(index + 1, 1);
            updateCycle(current_choice, current_set);
        }
        else
        {
            //  Previous cycle was still active, but user moved
            //  to another keypad key
            endCycle();
            current_choice = cycle_list.left(1);
            current_set = cycle_list;
            cycle_timer->start(cycle_time);
            startCycle(current_choice, current_set);
        }
    }
    else
    {
        //  First press with no cycle of any type active
        current_choice = cycle_list.left(1);
        current_set = cycle_list;
        startCycle(current_choice, current_set);
    }
}

void MythRemoteLineEdit::toggleShift()
{
    //  Toggle uppercase/lowercase and
    //  update the cycle display if it's
    //  active
    QString temp_choice = current_choice;
    QString temp_set = current_set;

    if (shift)
    {
        shift = false;
    }
    else
    {
        shift = true;
        temp_choice = current_choice.toUpper();
        temp_set = current_set.toUpper();
    }
    if (active_cycle)
    {
        updateCycle(temp_choice, temp_set);
    }
}

void MythRemoteLineEdit::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythRemoteLineEdit::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);
    emit gotFocus();    //perhaps we need to save a playlist?

    QColor highlight = palette().color(QPalette::Highlight);

    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QTextEdit::focusInEvent(e);
}

void MythRemoteLineEdit::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());

    if (popup && !popup->isHidden() && !popup->hasFocus())
        popup->hide();

    emit lostFocus();
    QTextEdit::focusOutEvent(e);
}

MythRemoteLineEdit::~MythRemoteLineEdit()
{
    if (m_actions)
        delete m_actions;

    Teardown();
}

void MythRemoteLineEdit::deleteLater(void)
{
    Teardown();
    QTextEdit::deleteLater();
}

void MythRemoteLineEdit::Teardown(void)
{
    if (cycle_timer)
    {
        cycle_timer->disconnect();
        cycle_timer->deleteLater();
        cycle_timer = NULL;
    }

    qt_delete(popup);
}

void MythRemoteLineEdit::popupVirtualKeyboard(void)
{
    qt_delete(popup);

    popup = new VirtualKeyboardQt(GetMythMainWindow(), this);
    GetMythMainWindow()->detach(popup);
    popup->exec();

    qt_delete(popup);
}

void MythRemoteLineEdit::insert(QString text)
{
    QTextEdit::insertPlainText(text);
    emit textChanged(toPlainText());
}

void MythRemoteLineEdit::del()
{
    textCursor().deleteChar();
    emit textChanged(toPlainText());
}

void MythRemoteLineEdit::backspace()
{
    textCursor().deletePreviousChar();
    emit textChanged(toPlainText());
}


static struct ActionDefStruct<MythPushButton> mpshbActions[] = {
    { "SELECT",   &MythPushButton::doSelect }
};
static int mpshbActionCount = NELEMS(mpshbActions);

MythPushButton::MythPushButton(QWidget *parent, const char *name) :
    QPushButton(parent),
    m_actions(new MythActions<MythPushButton>(this, mpshbActions,
                                              mpshbActionCount))
{
    setObjectName(name);
    setCheckable(false);
}

MythPushButton::MythPushButton(const QString &text, QWidget *parent) :
    QPushButton(text, parent),
    m_actions(new MythActions<MythPushButton>(this, mpshbActions,
                                              mpshbActionCount))
{
    setObjectName("MythPushButton");
    setCheckable(false);
}

MythPushButton::MythPushButton(const QString &ontext, const QString &offtext,
                               QWidget *parent, bool isOn) :
    QPushButton(ontext, parent),
    m_actions(new MythActions<MythPushButton>(this, mpshbActions,
                                              mpshbActionCount))
{
    onText = ontext;
    offText = offtext;

    setCheckable(true);

    if (isOn)
        setText(onText);
    else
        setText(offText);

    setChecked(isOn);
}

MythPushButton::~MythPushButton()
{
    if (m_actions)
        delete m_actions;
}

void MythPushButton::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

bool MythPushButton::doSelect(const QString &action)
{
    if (!isDown())
    {
        if (isCheckable())
            toggleText();
        setDown(true);
        emit pressed();
    }
    else
    {
        QKeyEvent tempe(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier, " ");
        QPushButton::keyReleaseEvent(&tempe);
    }
    return true;
}

void MythPushButton::keyPressEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions;
    keyPressActions.clear();

    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    if (!handled && !actions.isEmpty())
    {
        keyPressActions = actions;

        handled = m_actions->handleActions(actions);
    }

    if (!handled)
        QPushButton::keyPressEvent(e);
}

void MythPushButton::keyReleaseEvent(QKeyEvent *e)
{
    bool handled = false;
    QStringList actions = keyPressActions;

    handled = m_actions->handleActions(actions);

    if (!handled)
        QPushButton::keyReleaseEvent(e);
}

void MythPushButton::toggleText(void)
{
    if (!isCheckable())
        return;

    if (isChecked())
        setText(offText);
    else
        setText(onText);
}

void MythPushButton::focusInEvent(QFocusEvent *e)
{
    emit changeHelpText(helptext);

    QColor highlight = palette().color(QPalette::Highlight);
    QPalette palette;
    palette.setColor(backgroundRole(), highlight);
    setPalette(palette);

    QPushButton::focusInEvent(e);
}

void MythPushButton::focusOutEvent(QFocusEvent *e)
{
    setPalette(QPalette());
    QPushButton::focusOutEvent(e);
}

static struct ActionDefStruct<MythListBox> mlstbActions[] = {
    { "UP",       &MythListBox::doUp },
    { "DOWN",     &MythListBox::doDown },
    { "LEFT",     &MythListBox::doLeft },
    { "RIGHT",    &MythListBox::doRight },
    { "PAGEDOWN", &MythListBox::doPgDown },
    { "PAGEUP",   &MythListBox::doPgUp },
    { "0",        &MythListBox::doDigit },
    { "1",        &MythListBox::doDigit },
    { "2",        &MythListBox::doDigit },
    { "3",        &MythListBox::doDigit },
    { "4",        &MythListBox::doDigit },
    { "5",        &MythListBox::doDigit },
    { "6",        &MythListBox::doDigit },
    { "7",        &MythListBox::doDigit },
    { "8",        &MythListBox::doDigit },
    { "9",        &MythListBox::doDigit },
    { "PREVVIEW", &MythListBox::doPrevView },
    { "NEXTVIEW", &MythListBox::doNextView },
    { "MENU",     &MythListBox::doMenu },
    { "EDIT",     &MythListBox::doEdit },
    { "DELETE",   &MythListBox::doDelete },
    { "SELECT",   &MythListBox::doSelect }
};
static int mlstbActionCount = NELEMS(mlstbActions);

MythListBox::MythListBox(QWidget *parent, const QString &name) :
    QListWidget(parent),
    m_actions(new MythActions<MythListBox>(this, mlstbActions,
                                           mlstbActionCount))
{
    setObjectName(name);
    connect(this, SIGNAL(itemSelectionChanged()),
            this, SLOT(HandleItemSelectionChanged()));
}

MythListBox::~MythListBox()
{
    if (m_actions)
        delete m_actions;
}

void MythListBox::ensurePolished(void) const
{
    QListWidget::ensurePolished();

    QPalette pal = palette();
    QPalette::ColorRole  nR = QPalette::Highlight;
    QPalette::ColorGroup oA = QPalette::Active;
    QPalette::ColorRole  oR = QPalette::Button;
    pal.setColor(QPalette::Active,   nR, pal.color(oA,oR));
    pal.setColor(QPalette::Inactive, nR, pal.color(oA,oR));
    pal.setColor(QPalette::Disabled, nR, pal.color(oA,oR));

    const_cast<MythListBox*>(this)->setPalette(pal);
}

void MythListBox::setCurrentItem(const QString& matchText, bool caseSensitive,
                                 bool partialMatch)
{
    for (unsigned i = 0 ; i < (unsigned)count() ; ++i)
    {
        if (partialMatch)
        {
            if (caseSensitive)
            {
                if (text(i).startsWith(matchText))
                {
                    setCurrentRow(i);
                    break;
                }
            }
            else
            {
                if (text(i).toLower().startsWith(matchText.toLower()))
                {
                    setCurrentRow(i);
                    break;
                }
            }
        }
        else
        {
            if (caseSensitive)
            {
                if (text(i) == matchText)
                {
                    setCurrentRow(i);
                    break;
                }
            }
            else
            {
                if (text(i).toLower() == matchText.toLower())
                {
                    setCurrentRow(i);
                    break;
                }
            }
        }
    }
}

void MythListBox::HandleItemSelectionChanged(void)
{
    QList<QListWidgetItem*> items = QListWidget::selectedItems();
    int row = getIndex(items);
    if (row >= 0)
        emit highlighted(row);
}

void MythListBox::propagateKey(int key)
{
    QKeyEvent ev(QEvent::KeyPress, key, Qt::NoModifier);
    QListWidget::keyPressEvent(&ev);
}

bool MythListBox::doUp(const QString &action)
{
    // Qt::Key_Up at top of list allows focus to move to other widgets
    if (currentItem() == 0)
    {
        focusNextPrevChild(false);
        return true;
    }

    propagateKey(Qt::Key_Up);
    return true;
}

bool MythListBox::doDown(const QString &action)
{
    // Qt::Key_down at bottom of list allows focus to move to other widgets
    if (currentRow() == (int) count() - 1)
    {
        focusNextPrevChild(true);
        return true;
    }

    propagateKey(Qt::Key_Down);
    return true;
}

bool MythListBox::doLeft(const QString &action)
{
    focusNextPrevChild(false);
    return true;
}

bool MythListBox::doRight(const QString &action)
{
    focusNextPrevChild(true);
    return true;
}

bool MythListBox::doPgDown(const QString &action)
{
    propagateKey(Qt::Key_PageDown);
    return true;
}

bool MythListBox::doPgUp(const QString &action)
{
    propagateKey(Qt::Key_PageUp);
    return true;
}

bool MythListBox::doDigit(const QString &action)
{
    int percent = action.toInt() * 10;
    int nextItem = percent * count() / 100;
    if (!itemVisible(nextItem))
        setTopRow(nextItem);
    setCurrentRow(nextItem);
    return true;
}

bool MythListBox::doPrevView(const QString &action)
{
    int nextItem = currentRow();
    if (nextItem > 0)
        nextItem--;
    while (nextItem > 0 && text(nextItem)[0] == ' ')
        nextItem--;
    if (!itemVisible(nextItem))
        setTopRow(nextItem);
    setCurrentRow(nextItem);
    return true;
}

bool MythListBox::doNextView(const QString &action)
{
    int nextItem = currentRow();
    if (nextItem < (int)count() - 1)
        nextItem++;
    while (nextItem < (int)count() - 1 && text(nextItem)[0] == ' ')
        nextItem++;
    if (!itemVisible(nextItem))
        setTopRow(nextItem);
    setCurrentRow(nextItem);
    return true;
}

bool MythListBox::doMenu(const QString &action)
{
    emit menuButtonPressed(currentRow());
    return true;
}

bool MythListBox::doEdit(const QString &action)
{
    emit editButtonPressed(currentRow());
    return true;
}

bool MythListBox::doDelete(const QString &action)
{
    emit deleteButtonPressed(currentRow());
    return true;
}

bool MythListBox::doSelect(const QString &action)
{
    emit accepted(currentRow());
    return true;
}

void MythListBox::keyPressEvent(QKeyEvent* e)
{
    bool handled = false;
    QStringList actions;
    handled = GetMythMainWindow()->TranslateKeyPress("qt", e, actions);

    if (!handled)
        handled = m_actions->handleActions(actions);

    if (!handled)
        e->ignore();
}

void MythListBox::setHelpText(const QString &help)
{
    bool changed = helptext != help;
    helptext = help;
    if (hasFocus() && changed)
        emit changeHelpText(help);
}

void MythListBox::focusOutEvent(QFocusEvent *e)
{
    QPalette pal = palette();
    QPalette::ColorRole  nR = QPalette::Highlight;
    QPalette::ColorGroup oA = QPalette::Active;
    QPalette::ColorRole  oR = QPalette::Button;
    pal.setColor(QPalette::Active,   nR, pal.color(oA,oR));
    pal.setColor(QPalette::Inactive, nR, pal.color(oA,oR));
    pal.setColor(QPalette::Disabled, nR, pal.color(oA,oR));
    setPalette(pal);
    QListWidget::focusOutEvent(e);
}

void MythListBox::focusInEvent(QFocusEvent *e)
{
    setPalette(QPalette());

    emit changeHelpText(helptext);
    QListWidget::focusInEvent(e);
}


void MythListBox::setTopRow(uint row)
{
    QListWidgetItem *widget = item(row);
    if (widget)
        scrollToItem(widget, QAbstractItemView::PositionAtTop);
}

void MythListBox::insertItem(const QString &label)
{
    addItem(label);
}

void MythListBox::insertStringList(const QStringList &label_list)
{
    addItems(label_list);
}

void MythListBox::removeRow(uint row)
{
    QListWidgetItem *widget = takeItem(row);
    if (widget)
        delete widget;
}

void MythListBox::changeItem(const QString &new_text, uint row)
{
    QListWidgetItem *widget = item(row);
    if (widget)
        widget->setText(new_text);
}

int MythListBox::getIndex(const QList<QListWidgetItem*> &list)
{
    return (list.empty()) ? -1 : row(list[0]);
}

QString MythListBox::text(uint row) const
{
    QListWidgetItem *widget = item(row);
    return (widget) ? widget->text() : QString::null;
}

bool MythListBox::itemVisible(uint row) const
{
    QListWidgetItem *widget = item(row);
    return (widget) ? !isItemHidden(widget) : false;
}

/* vim: set expandtab tabstop=4 shiftwidth=4: */
