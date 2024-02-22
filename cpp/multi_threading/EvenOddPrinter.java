public class EvenOddPrinter {

    public static void main(String[] args) {
        // Create two instances of Runnable implementing even and odd number printing logic
        Runnable evenRunnable = () -> printEvenNumbers();
        Runnable oddRunnable = () -> printOddNumbers();

        // Create two threads using the above Runnables
        Thread evenThread = new Thread(evenRunnable);
        Thread oddThread = new Thread(oddRunnable);

        // Start both threads
        evenThread.start();
        oddThread.start();
    }

    private static void printEvenNumbers() {
        for (int i = 2; i <= 20; i += 2) {
            System.out.println(Thread.currentThread().getName() + ": " + i);
            try {
                // Sleep for a short time to simulate some work
                Thread.sleep(50);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    private static void printOddNumbers() {
        for (int i = 1; i <= 20; i += 2) {
            System.out.println(Thread.currentThread().getName() + ": " + i);
            try {
                // Sleep for a short time to simulate some work
                Thread.sleep(50);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }
}

