package org.chromium.base.test.asynctask;

import org.chromium.base.AsyncTask;
import org.robolectric.annotation.internal.DoNotInstrument;
import org.robolectric.util.ReflectionHelpers;
import org.robolectric.util.ReflectionHelpers.ClassParameter;

/**
 * Bridge between shadows and {@link org.chromium.base.AsyncTask}.
 */
@DoNotInstrument
public class ShadowAsyncTaskBridge<Params, Progress, Result> {
    private AsyncTask<Params, Progress, Result> asyncTask;

    public ShadowAsyncTaskBridge(AsyncTask<Params, Progress, Result> asyncTask) {
        this.asyncTask = asyncTask;
    }

    @SuppressWarnings("unchecked")
    public Result doInBackground(Params... params) {
        return ReflectionHelpers.callInstanceMethod(
                asyncTask, "doInBackground", ClassParameter.from(Object[].class, params));
    }

    public void onPreExecute() {
        ReflectionHelpers.callInstanceMethod(asyncTask, "onPreExecute");
    }

    public void onPostExecute(Result result) {
        ReflectionHelpers.callInstanceMethod(
                asyncTask, "onPostExecute", ClassParameter.from(Object.class, result));
    }

    @SuppressWarnings("unchecked")
    public void onProgressUpdate(Progress... values) {
        ReflectionHelpers.callInstanceMethod(
                asyncTask, "onProgressUpdate", ClassParameter.from(Object[].class, values));
    }

    public void onCancelled() {
        ReflectionHelpers.callInstanceMethod(asyncTask, "onCancelled");
    }
}
