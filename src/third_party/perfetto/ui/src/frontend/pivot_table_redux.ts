import * as m from 'mithril';

import {Actions} from '../common/actions';
import {ColumnType} from '../common/query_result';
import {
  Area,
  PivotTableReduxQuery,
  PivotTableReduxResult
} from '../common/state';
import {PivotTree} from '../controller/pivot_table_redux_controller';

import {globals} from './globals';
import {Panel} from './panel';
import {
  aggregationIndex,
  ColumnSet,
  generateQuery,
  QueryGeneratorError,
  sliceAggregationColumns,
  Table,
  TableColumn,
  tables,
  threadSliceAggregationColumns
} from './pivot_table_redux_query_generator';

interface ColumnSetCheckboxAttrs {
  set: ColumnSet;
  setKey: TableColumn;
}

interface PathItem {
  tree: PivotTree;
  nextKey: ColumnType;
}

// Helper component that controls whether a particular key is present in a
// ColumnSet.
class ColumnSetCheckbox implements m.ClassComponent<ColumnSetCheckboxAttrs> {
  view({attrs}: m.Vnode<ColumnSetCheckboxAttrs>) {
    return m('input[type=checkbox]', {
      onclick: (e: InputEvent) => {
        const target = e.target as HTMLInputElement;
        if (target.checked) {
          attrs.set.add(attrs.setKey);
        } else {
          attrs.set.delete(attrs.setKey);
        }
        globals.rafScheduler.scheduleFullRedraw();
      },
      checked: attrs.set.has(attrs.setKey)
    });
  }
}

interface PivotTableReduxAttrs {
  selectionArea: Area;
}

export class PivotTableRedux extends Panel<PivotTableReduxAttrs> {
  selectedPivotsMap = new ColumnSet();
  selectedAggregations = new ColumnSet();
  constrainToArea = true;
  editMode = true;

  renderCanvas(): void {}

  generateQuery(attrs: PivotTableReduxAttrs): PivotTableReduxQuery {
    return generateQuery(
        this.selectedPivotsMap,
        this.selectedAggregations,
        attrs.selectionArea,
        this.constrainToArea);
  }

  runQuery(attrs: PivotTableReduxAttrs) {
    try {
      const query = this.generateQuery(attrs);
      const lastPivotTableState = globals.state.pivotTableRedux;
      globals.dispatch(Actions.setPivotStateReduxState({
        pivotTableState: {
          query,
          queryId: lastPivotTableState.queryId + 1,
          selectionArea: lastPivotTableState.selectionArea,
          queryResult: null
        }
      }));
    } catch (e) {
      console.log(e);
    }
  }

  renderTablePivotColumns(t: Table) {
    return m(
        'li',
        t.name,
        m('ul',
          t.columns.map(
              col =>
                  m('li',
                    m(ColumnSetCheckbox, {
                      set: this.selectedPivotsMap,
                      setKey: [t.name, col],
                    }),
                    col))));
  }

  renderResultsView() {
    return m(
        '.pivot-table-redux',
        m('button.mode-button',
          {
            onclick: () => {
              this.editMode = true;
              globals.rafScheduler.scheduleFullRedraw();
            }
          },
          'Edit'),
        this.renderResultsTable());
  }

  renderSectionRow(
      path: PathItem[], tree: PivotTree,
      result: PivotTableReduxResult): m.Vnode {
    const renderedCells = [];
    for (let j = 0; j + 1 < path.length; j++) {
      renderedCells.push(m('td', m('span.indent', ' '), `${path[j].nextKey}`));
    }

    const treeDepth = result.metadata.pivotColumns.length;
    const colspan = treeDepth - path.length + 1;
    const button =
        m('button',
          {
            onclick: () => {
              tree.isCollapsed = !tree.isCollapsed;
              globals.rafScheduler.scheduleFullRedraw();
            }
          },
          m('i.material-icons',
            tree.isCollapsed ? 'expand_more' : 'expand_less'));

    renderedCells.push(
        m('td', {colspan}, button, `${path[path.length - 1].nextKey}`));

    for (const value of tree.aggregates) {
      renderedCells.push(m('td', `${value}`));
    }

    return m('tr', renderedCells);
  }

  renderTree(
      path: PathItem[], tree: PivotTree, result: PivotTableReduxResult,
      sink: m.Vnode[]) {
    if (tree.isCollapsed) {
      sink.push(this.renderSectionRow(path, tree, result));
      return;
    }
    if (tree.children.size > 0) {
      // Avoid rendering the intermediate results row for the root of tree
      // and in case there's only one child subtree.
      if (!tree.isCollapsed && path.length > 0 && tree.children.size !== 1) {
        sink.push(this.renderSectionRow(path, tree, result));
      }
      for (const [key, childTree] of tree.children.entries()) {
        path.push({tree: childTree, nextKey: key});
        this.renderTree(path, childTree, result, sink);
        path.pop();
      }
      return;
    }

    // Avoid rendering the intermediate results row if it has only one leaf
    // row.
    if (!tree.isCollapsed && path.length > 0 && tree.rows.length > 1) {
      sink.push(this.renderSectionRow(path, tree, result));
    }
    for (const row of tree.rows) {
      const renderedCells = [];
      const treeDepth = result.metadata.pivotColumns.length;
      for (let j = 0; j < treeDepth; j++) {
        if (j < path.length) {
          renderedCells.push(m('td', m('span.indent', ' '), `${row[j]}`));
        } else {
          renderedCells.push(m(`td`, `${row[j]}`));
        }
      }
      for (let j = 0; j < result.metadata.aggregationColumns.length; j++) {
        const value = row[aggregationIndex(treeDepth, j, treeDepth)];
        renderedCells.push(m('td', `${value}`));
      }

      sink.push(m('tr', renderedCells));
    }
  }

  renderTotalsRow(queryResult: PivotTableReduxResult) {
    const overallValuesRow =
        [m('td.total-values',
           {'colspan': queryResult.metadata.pivotColumns.length},
           m('strong', 'Total values:'))];
    for (const aggValue of queryResult.tree.aggregates) {
      overallValuesRow.push(m('td', `${aggValue}`));
    }
    return m('tr', overallValuesRow);
  }

  renderResultsTable() {
    const state = globals.state.pivotTableRedux;
    if (state.query !== null || state.queryResult === null) {
      return m('div', 'Loading...');
    }

    const renderedRows: m.Vnode[] = [];
    const tree = state.queryResult.tree;

    if (tree.children.size === 0 && tree.rows.length === 0) {
      // Empty result, render a special message
      return m('.empty-result', 'No slices in the current selection.');
    }

    this.renderTree([], tree, state.queryResult, renderedRows);

    const allColumns = state.queryResult.metadata.pivotColumns.concat(
        state.queryResult.metadata.aggregationColumns);
    return m(
        'table.query-table.pivot-table',
        m('thead', m('tr', allColumns.map(column => m('td', column)))),
        m('tbody', this.renderTotalsRow(state.queryResult), renderedRows));
  }

  renderQuery(attrs: PivotTableReduxAttrs): m.Vnode {
    // Prepare a button to switch to results mode.
    let innerElement =
        m('button.mode-button',
          {
            onclick: () => {
              this.editMode = false;
              this.runQuery(attrs);
              globals.rafScheduler.scheduleFullRedraw();
            }
          },
          'Execute');
    try {
      this.generateQuery(attrs);
    } catch (e) {
      if (e instanceof QueryGeneratorError) {
        // If query generation fails, show an error message instead of a button.
        innerElement = m('div.query-error', e.message);
      } else {
        throw e;
      }
    }

    return m(
        'div',
        m('div',
          m('input', {
            type: 'checkbox',
            id: 'constrain-to-selection',
            checked: this.constrainToArea,
            onclick: (e: InputEvent) => {
              const checkbox = e.target as HTMLInputElement;
              this.constrainToArea = checkbox.checked;
            }
          }),
          m('label',
            {
              'for': 'constrain-to-selection',
            },
            'Constrain to current time range')),
        innerElement);
  }

  view({attrs}: m.Vnode<PivotTableReduxAttrs>) {
    return this.editMode ? this.renderEditView(attrs) :
                           this.renderResultsView();
  }

  renderEditView(attrs: PivotTableReduxAttrs) {
    return m(
        '.pivot-table-redux.edit',
        m('div',
          m('h2', 'Pivots'),
          m('ul',
            tables.map(
                t => this.renderTablePivotColumns(t),
                ))),
        m('div',
          m('h2', 'Aggregations'),
          m('ul',
            ...sliceAggregationColumns.map(
                t =>
                    m('li',
                      m(ColumnSetCheckbox, {
                        set: this.selectedAggregations,
                        setKey: ['slice', t],
                      }),
                      t)),
            ...threadSliceAggregationColumns.map(
                t =>
                    m('li',
                      m(ColumnSetCheckbox, {
                        set: this.selectedAggregations,
                        setKey: ['thread_slice', t],
                      }),
                      `thread_slice.${t}`)))),
        this.renderQuery(attrs));
  }
}