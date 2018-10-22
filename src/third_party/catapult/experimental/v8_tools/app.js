//  Vue component for drop-down menu; here the metrics,
//  stories and diagnostics are chosen through selection.
'use strict';
const app = new Vue({
  el: '#app',
  data: {
    sampleArr: [],
    guidValue: null,
    selected_metric: null,
    selected_story: null,
    selected_diagnostic: null,
    graph: new GraphData(),
    searchQuery: '',
    gridColumns: ['id', 'metric', 'averageSampleValues'],
    gridData: [],
    parsedMetrics: null,
    globalDiagnostic: null,
    columnsForChosenDiagnostic: null,
    resetDropDownMenu: false,
    oldGridData: []
  },

  methods: {
    plotBarChart(data) {
      this.graph.xAxis('Story')
          .yAxis('Memory used (MiB)')
          .title(this.globalDiagnostic
              .charAt(0).toUpperCase() + this.globalDiagnostic.slice(1))
          .setData(data)
          .plotBar();
    },
    //  Draw a cumulative frequency plot depending on the target value.
    //  This is for displaying results for the selected parameters
    // from the drop-down menu.
    plotCumulativeFrequency() {
      this
          .plotCumulativeFrequencyPlot(JSON
              .parse(JSON.stringify((this.filteredData))),
          this.selected_story);
    },

    //  Draw a dot plot depending on the target value.
    //  This is mainly for results from the table.
    plotDotPlot(target, story, traces) {
      const openTrace = (label, index) => {
        window.open(traces[label][index]);
      };
      this.graph
          .xAxis('Memory used (MiB)')
          .title(story)
          .setData(target, openTrace)
          .plotDot();
    },

    //  Draw a cumulative frequency plot depending on the target value.
    //  This is mainly for the results from the table.
    plotCumulativeFrequencyPlot(target, story) {
      this.graph.xAxis('Cumulative frequency')
          .yAxis('Memory used (MiB)')
          .title(story)
          .setData(target)
          .plotCumulativeFrequency();
    },

    //  Being given a metric, a story, a diagnostic and a set of
    //  subdiagnostics (for example, 3 labels from the total available
    //  ones), the method return the sample values for each subdiagnostic.
    getSubdiagnostics(
        getTargetValueFromSample, metric, story, diagnostic, diagnostics) {
      const result = this.sampleArr
          .filter(value => value.name === metric &&
          this.guidValue
              .get(value.diagnostics.stories)[0] ===
              story);

      const content = new Map();
      for (const val of result) {
        const diagnosticItem = this.guidValue.get(
            val.diagnostics[diagnostic]);
        if (diagnosticItem === undefined) {
          continue;
        }
        let currentDiagnostic = '';
        if (typeof diagnosticItem === 'number') {
          currentDiagnostic = diagnosticItem;
        } else {
          currentDiagnostic = diagnosticItem[0];
        }
        const targetValue = getTargetValueFromSample(val);
        if (content.has(currentDiagnostic)) {
          const aux = content.get(currentDiagnostic);
          content.set(currentDiagnostic, aux.concat(targetValue));
        } else {
          content.set(currentDiagnostic, targetValue);
        }
      }
      const obj = {};
      for (const [key, value] of content.entries()) {
        if (diagnostics === undefined ||
          diagnostics.includes(key.toString())) {
          obj[key] = value;
        }
      }
      return obj;
    },

    getSampleValues(sample) {
      const toMiB = (x) => (x / MiB).toFixed(5);
      const values = sample.sampleValues;
      return values.map(value => toMiB(value));
    },
    //  Draw a plot by default with all the sub-diagnostics
    //  in the same plot;
    plotSingleMetricWithAllSubdiagnostics(metric, story, diagnostic) {
      const obj = this.getSubdiagnostics(
          this.getSampleValues, metric, story, diagnostic);
      this.plotCumulativeFrequencyPlot(obj, story);
    },

    //  Draw a plot depending on the target value which is made
    //  of a metric, a story, a diagnostic and a couple of sub-diagnostics
    //  and the chosen type of plot. All are chosen from the table.
    plotSingleMetric(metric, story, diagnostic,
        diagnostics, chosenPlot) {
      const target = this.targetForMultipleDiagnostics(
          this.getSampleValues, metric, story, diagnostic, diagnostics);
      if (chosenPlot === 'Dot plot') {
        const getTraceLinks = (sample) => {
          const traceId = sample.diagnostics.traceUrls;
          return this.guidValue.get(traceId);
        };
        const traces = this.targetForMultipleDiagnostics(
            getTraceLinks, metric, story, diagnostic, diagnostics);
        this.plotDotPlot(target, story, traces);
      } else {
        this.plotCumulativeFrequencyPlot(target, story);
      }
    },

    //  Compute the target when the metric, story, diagnostics and
    //  sub-diagnostics are chosen from the table, not from the drop-down menu.
    //  It should be the same for both components but for now they should
    //  be divided.
    targetForMultipleDiagnostics(
        getTargetValueFromSample, metric, story, diagnostic, diagnostics) {
      if (metric === null || story === null ||
        diagnostic === null || diagnostics === null) {
        return undefined;
      }
      return this.getSubdiagnostics(
          getTargetValueFromSample, metric, story, diagnostic, diagnostics);
    }
  },

  computed: {
    gridDataLoaded() {
      return this.gridData.length > 0;
    },
    data_loaded() {
      return this.sampleArr.length > 0;
    },

    seen_stories() {
      return this.stories && this.stories.length > 0;
    },

    seen_diagnostics() {
      return this.diagnostics && this.diagnostics.length > 0;
    },

    //  Compute the metrics for the drop-down menu;
    //  The user will chose one of them.
    metrics() {
      if (this.parsedMetrics === null ||
        this.resetDropDownMenu === true) {
        const metricsNames = [];
        this.sampleArr.map(el => metricsNames.push(el.name));
        return _.uniq(metricsNames);
      }
      return this.parsedMetrics;
    },
    //  Compute the stories depending on the chosen metric.
    //  The user should chose one of them.
    stories() {
      const reqMetrics = this.sampleArr
          .filter(elem => elem.name === this.selected_metric);
      const storiesByGuid = [];
      for (const elem of reqMetrics) {
        let storyName = this.guidValue.get(elem.diagnostics.stories);
        if (storyName === undefined) {
          continue;
        }
        if (typeof storyName !== 'number') {
          storyName = storyName[0];
        }
        storiesByGuid.push(storyName);
      }
      return _.uniq(storiesByGuid);
    },

    //  Compute all diagnostic elements; the final result will actually
    //  depend on the metric, the story and this diagnostic.
    diagnostics() {
      if (this.selected_story !== null && this.selected_metric !== null) {
        const result = this.sampleArr
            .filter(value => value.name === this.selected_metric &&
                    this.guidValue
                        .get(value.diagnostics.stories)[0] ===
                        this.selected_story);
        const allDiagnostics = result.map(val => Object.keys(val.diagnostics));
        return _.union.apply(this, allDiagnostics);
      }
    },

    //  Compute the final result with the chosen metric, story and diagnostics.
    //  These are chosen from the drop-down menu.
    filteredData() {
      if (this.selected_story === null ||
        this.selected_metric === null ||
        this.selected_diagnostic === null) {
        return undefined;
      }
      return this
          .getSubdiagnostics(this.selected_metric,
              this.selected_story,
              this.selected_diagnostic);
    },

    //  Extract all diagnostic names from all elements.
    allDiagnostics() {
      if (this.sampleArr === undefined) {
        return undefined;
      }
      const allDiagnostics = this.sampleArr
          .map(val => Object.keys(val.diagnostics));
      return _.union.apply(this, allDiagnostics);
    },
  },

  watch: {
    //  Whenever a new metric/ story/ diagnostic is chosen
    //  this function will run for drawing a new type of plot.
    //  These items are chosen from the drop-down menu.
    filteredData() {
      this.plotCumulativeFrequency();
    },

    metrics() {
      this.selected_metric = null;
      this.selected_story = null;
      this.selected_diagnostic = null;
    },
    //  Compute the data for the columns after the user has chosen a
    //  particular global diagnostic that has to be split in
    //  multiple subdiagnostics.
    globalDiagnostic() {
      if (this.globalDiagnostic === null) {
        return undefined;
      }
      this.gridColumns = ['id', 'metric', 'averageSampleValues'];
      const newDiagnostics = new Set();
      const metricTodiagnoticValuesMap = new Map();
      for (const elem of this.sampleArr) {
        let currentDiagnostic = this.guidValue.
            get(elem.diagnostics[this.globalDiagnostic]);
        if (currentDiagnostic === undefined) {
          continue;
        }
        if (currentDiagnostic !== 'number') {
          currentDiagnostic = currentDiagnostic[0];
        }
        newDiagnostics.add(currentDiagnostic);

        if (!metricTodiagnoticValuesMap.has(elem.name)) {
          const map = new Map();
          map.set(currentDiagnostic, [average(elem.sampleValues)]);
          metricTodiagnoticValuesMap.set(elem.name, map);
        } else {
          const map = metricTodiagnoticValuesMap.get(elem.name);
          if (map.has(currentDiagnostic)) {
            const array = map.get(currentDiagnostic);
            array.push(average(elem.sampleValues));
            map.set(currentDiagnostic, array);
            metricTodiagnoticValuesMap.set(elem.name, map);
          } else {
            map.set(currentDiagnostic, [average(elem.sampleValues)]);
            metricTodiagnoticValuesMap.set(elem.name, map);
          }
        }
      }
      this.columnsForChosenDiagnostic = Array.from(newDiagnostics);
      for (const elem of this.gridData) {
        if (metricTodiagnoticValuesMap.get(elem.metric) === undefined) {
          continue;
        }
        for (const diagnostic of Array.from(newDiagnostics)) {
          elem[diagnostic] = average(metricTodiagnoticValuesMap
              .get(elem.metric).get(diagnostic));
        }
      }
    },

    //  Whenever we have new inputs from the menu (parsed inputs that
    //  where obtained by choosing from the tree) these should be
    //  added in the table (adding the average sample value)
    parsedMetrics() {
      const metricAverage = new Map();
      for (const e of this.sampleArr) {
        if (this.parsedMetrics.includes(e.name)) {
          if (metricAverage.has(e.name)) {
            const aux = metricAverage.get(e.name);
            aux.push(average(e.sampleValues));
            metricAverage.set(e.name, aux);
          } else {
            metricAverage.set(e.name, [average(e.sampleValues)]);
          }
        }
      }
      const tableElems = [];
      let id = 1;
      for (const [key, value] of metricAverage.entries()) {
        tableElems.push({
          id: id++,
          metric: key,
          averageSampleValues: average(value)
        });
      }
      this.gridData = tableElems;
    }
  }
});
