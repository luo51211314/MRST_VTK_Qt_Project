#include "ParamDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMessageBox>

static QString csvEscape(QString s)
{
    s.replace("\"", "\"\"");
    if (s.contains(',') || s.contains('\n') || s.contains('\r') || s.contains('"'))
        s = "\"" + s + "\"";
    return s;
}

ParamDialog::ParamDialog(QWidget* parent)
    : QDialog(parent)
{
    QStringList fixedParamNames = {
        "X方向域长度Lx",
        "Y方向域长度Ly",
        "Z方向域长度Lz",
        "X方向网格数Nx",
        "Y方向网格数Ny",
        "Z方向网格数Nz",
        "孔隙度phi",
        "X方向渗透率K[0]",
        "Y方向渗透率K[1]",
        "Z方向渗透率K[2]",
        "水的粘度mu_w",
        "油的粘度mu_o",
        "气的粘度mu_g",
        "水的压缩系数cw",
        "油的压缩系数co",
        "气的压缩系数cg",
        "参考压力P_ref",
        "束缚水饱和度Swi",
        "剩余油饱和度Sor",
        "临界气饱和度Sgc",
        "水相指数nw",
        "气相指数ng",
        "油相指数no",
        "最大倾角",
        "裂缝长度分布范围",
        "随机裂缝数量",
        "天然裂缝开度m",
        "天然裂缝渗透率D",
        "水力裂缝总长hf_len",
        "水力裂缝高度hf_height",
        "水力裂缝开度f.aperture",
        "水力裂缝渗透率f.perm",
        "初始压力P",
        "初始水饱和度Sw",
        "初始气饱和度Sg",
        "井指数（生产率系数）WI",
        "井底流动压力P_bhp",
        "天数",



    };



    setWindowTitle(tr("参数输入"));
    setModal(true);
    resize(650, 420);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    table = new QTableWidget(this);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({tr("参数名"), tr("参数值")});

    auto* header = table->horizontalHeader();

    header->setStretchLastSection(false);

    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->setSectionResizeMode(1, QHeaderView::Stretch);

    table->setColumnWidth(0, 220);


    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::EditKeyPressed
                           | QAbstractItemView::SelectedClicked);

    root->addWidget(table, 1);

    // 顶部/底部加“新增/删除行”
    auto* rowBar = new QHBoxLayout();
    auto* btnAdd = new QPushButton(tr("新增参数"), this);
    auto* btnDel = new QPushButton(tr("删除选中"), this);
    rowBar->addWidget(btnAdd);
    rowBar->addWidget(btnDel);
    rowBar->addStretch();
    root->addLayout(rowBar);

    connect(btnAdd, &QPushButton::clicked, this, [this](){
        const int r = table->rowCount();
        addRow("", "", false);   // 新增的，一定是“未确定参数”
        table->setCurrentCell(r, 0);
        table->editItem(table->item(r, 0));
    });



    connect(btnDel, &QPushButton::clicked, this, [this](){
        auto ranges = table->selectedRanges();
        if (ranges.isEmpty()) return;
        // 删除选中行（从后往前删）
        for (int r = ranges.first().bottomRow(); r >= ranges.first().topRow(); --r)
            table->removeRow(r);
    });

    // 默认先给3行空参数，方便用户直接填
    //addRow();
    //addRow();
    //addRow();
    for (const auto& name : fixedParamNames) {
        addRow(name, "", /*isFixed=*/true);
    }


    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    box->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    box->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    root->addWidget(box);

    connect(box, &QDialogButtonBox::accepted, this, [this](){
        // 简单校验：参数名不能为空（允许值为空）
        QSet<QString> names;
        for (int r = 0; r < table->rowCount(); ++r) {
            auto* itName = table->item(r, 0);
            auto* itVal  = table->item(r, 1);

            if (!itName || !itVal) continue;

            QString name = itName->text().trimmed();
            QString val  = itVal->text().trimmed();

            // 跳过全空行
            if (name.isEmpty() && val.isEmpty())
                continue;

            // 参数名不能为空
            if (name.isEmpty()) {
                QMessageBox::warning(this, tr("提示"),
                                     tr("第 %1 行参数名为空").arg(r + 1));
                return;
            }

            // 参数名不能重复（非常重要）
            if (names.contains(name)) {
                QMessageBox::warning(this, tr("提示"),
                                     tr("参数名重复：%1").arg(name));
                return;
            }
            names.insert(name);
        }

        accept();
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ParamDialog::addRow(const QString& name,
                         const QString& value,
                         bool isFixed)
{
    const int r = table->rowCount();
    table->insertRow(r);

    auto* itName = new QTableWidgetItem(name);
    auto* itVal  = new QTableWidgetItem(value);

    if (isFixed) {
        // 1️⃣ 已确定参数：参数名不可编辑
        itName->setFlags(itName->flags() & ~Qt::ItemIsEditable);

        // 2️⃣ 打一个标记：这是系统参数
        itName->setData(Qt::UserRole, 0);
    } else {
        // 未确定 / 用户参数
        itName->setData(Qt::UserRole, 1);
    }

    table->setItem(r, 0, itName);
    table->setItem(r, 1, itVal);
}


QVector<QPair<QString, QString>> ParamDialog::params() const
{
    QVector<QPair<QString, QString>> out;
    for (int r = 0; r < table->rowCount(); ++r) {
        const QString name = table->item(r,0) ? table->item(r,0)->text().trimmed() : "";
        const QString val  = table->item(r,1) ? table->item(r,1)->text().trimmed() : "";
        if (name.isEmpty() && val.isEmpty()) continue;
        out.push_back({name, val});
    }
    return out;
}

QString ParamDialog::toCsvText() const
{
    QString csv = "name,value\n";
    for (auto& p : params()) {
        csv += csvEscape(p.first) + "," + csvEscape(p.second) + "\n";
    }
    return csv;
}
