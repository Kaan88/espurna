/**
 * @typedef {function(string): boolean} Question
 */

/**
 * @typedef {function(Question): boolean} QuestionWrapper
 */

/**
 * @param {QuestionWrapper[]} questions
 * @param {function(): void} callback
 */
export function askAndCall(questions, callback) {
    for (let question of questions) {
        if (!question(window.confirm)) {
            return;
        }
    }

    callback();
}
